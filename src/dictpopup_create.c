#include <stdio.h>
#include <unistd.h> // access
#include <sys/stat.h> // mkdir
#include <signal.h> // Catch SIGINT

#include <dirent.h>
#include <zip.h>
#include <glib.h> // for g_get_user_data_dir()

#include "util.h"
#include "pdjson.h"
#include "dbwriter.h"
#include "settings.h"
#include "messages.h"

#define error_return(retval, ...)   \
	do {                        \
	    error_msg(__VA_ARGS__); \
	    return retval;          \
	} while (0)

volatile sig_atomic_t sigint_received = 0;

const char json_typename[][16] = {
    [JSON_ERROR] = "ERROR",
    [JSON_DONE] = "DONE",
    [JSON_OBJECT] = "OBJECT",
    [JSON_OBJECT_END] = "OBJECT_END",
    [JSON_ARRAY] = "ARRAY",
    [JSON_ARRAY_END] = "ARRAY_END",
    [JSON_STRING] = "STRING",
    [JSON_NUMBER] = "NUMBER",
    [JSON_TRUE] = "TRUE",
    [JSON_FALSE] = "FALSE",
    [JSON_NULL] = "NULL",
};

static void append_definition(json_stream s[static 1], stringbuilder_s sb[static 1], s8 liststyle, i32 listdepth);

static s8
get_default_path(void) // Where db is stored
{
    return buildpath(fromcstr_((char*)g_get_user_data_dir()), S("dictpopup"));
}

s8
unzip_file(zip_t* archive, const char* filename)
{
    struct zip_stat finfo;
    zip_stat_init(&finfo);
    if (zip_stat(archive, filename, 0, &finfo) == -1)
	error_return((s8){ 0 }, "Error stat'ing file '%s': %s\n", filename, zip_strerror(archive));

    s8 buffer = news8(finfo.size);     // automatically null-terminated

    struct zip_file* index_file = zip_fopen(archive, filename, 0);
    if (!index_file)
	error_return((s8){ 0 }, "Error opening file '%s': %s\n", filename, zip_strerror(archive));

    buffer.len = zip_fread(index_file, buffer.s, buffer.len);
    if (buffer.len == -1)
	error_return((s8){ 0 }, "Could not read file '%s': %s", filename, zip_strerror(archive));
    if ((size_t)buffer.len != finfo.size)
	debug_msg("Did not read all of %s!", filename);

    zip_fclose(index_file);
    return buffer;
}


static s8
json_get_string_(json_stream* json)
{
    size_t slen = 0;
    s8 r = { 0 };
    r.s = (u8*)json_get_string(json, &slen);
    r.len = slen > 0 ? (size)(slen - 1) : 0;  // API includes terminating 0 in length
    return r;
}

static void
add_dictent_to_db(dictentry de)
{
    if (de.definition.len == 0)
	debug_msg("Could not parse a definition for entry with kanji: \"%s\" and reading: \"%s\". In dictionary: \"%s\"",
		  de.kanji.s, de.reading.s, de.dictname.s);
    else if (de.kanji.len == 0 && de.reading.len == 0)
	debug_msg("Could not obtain kanji nor reading for entry with definition: \"%s\", in dictionary: \"%s\"",
		  de.definition.s, de.dictname.s);
    else
    {
	s8 sep = S("\0");
	s8 datastr = concat(de.dictname, sep, de.kanji, sep, de.reading, sep, de.definition);

	if (de.kanji.len > 0)
	    addtodb(de.kanji, datastr);
	if (de.reading.len > 0)         // Let the db figure out duplicates
	    addtodb(de.reading, datastr);

	free(datastr.s);
    }
}

static s8
parse_liststyletype(s8 lst)
{
    if (startswith(lst, S("\"")) && endswith(lst, S("\"")))
	return s8dup(cuttail(cuthead(lst, 1), 1));
    else if (s8equals(lst, S("circle")))
	return S("◦");
    else if (s8equals(lst, S("square")))
	return S("▪");
    else if (s8equals(lst, S("none")))
	return S(" ");
    else
	return s8dup(lst); // そのまま
}

enum tag_type { TAG_UNKNOWN, TAG_DIV, TAG_SPAN, TAG_UL, TAG_OL, TAG_LI };

static void
append_structured_content(json_stream* s, stringbuilder_s sb[static 1], s8 liststyle, i32 listdepth)
{
    enum json_type type;
    enum tag_type tag = TAG_UNKNOWN;
    for (;;)
    {
	type = json_next(s);

	if (type == JSON_OBJECT_END || type == JSON_DONE)
	    break;
	else if (type == JSON_ERROR)
	{
	    error_msg("%s", json_get_error(s));
	    break;
	}
	else if (type == JSON_STRING)
	{
	    s8 value = json_get_string_(s);

	    if (s8equals(value, S("tag")))
	    {
		type = json_next(s);
		assert(type == JSON_STRING);
		value = json_get_string_(s);

		// Content needs to come last for this to work
		if (s8equals(value, S("div")))
		    tag = TAG_DIV;
		else if (s8equals(value, S("ul")))
		{
		    tag = TAG_UL;
		    listdepth++;

		    liststyle = S("•"); //default
		}
		else if (s8equals(value, S("ol")))
		{
		    tag = TAG_OL;
		    listdepth++;
		}
		else if (s8equals(value, S("li")))
		{
		    tag = TAG_LI;
		    //TODO: Default numbers for tag ol
		}
		else
		    tag = TAG_UNKNOWN;
	    }
	    else if ((tag == TAG_UL || tag == TAG_LI) && s8equals(value, S("style")))
	    {
		//TODO: json_peek for the very unlikely event that there is an array
		type = json_next(s);

		if (type == JSON_OBJECT)
		{
		    type = json_next(s);
		    while (type != JSON_OBJECT_END && type != JSON_DONE && type != JSON_ERROR)
		    {
			if (type == JSON_STRING && s8equals(json_get_string_(s), S("listStyleType")))
			{
			    type = json_next(s);
			    assert(type == JSON_STRING);
			    liststyle = parse_liststyletype(json_get_string_(s));
			}
			else
			    json_skip(s);

			type = json_next(s);
		    }
		}
		else if (type == JSON_ARRAY)
		    assert(0);
		else
		    debug_msg("Could not parse style of list. Skipping..");
	    }
	    else if (s8equals(value, S("content")))
	    {
		if (tag == TAG_DIV || tag == TAG_LI)
		{
		    if (sb->len != 0 && sb->data[sb->len - 1] != '\n')
			sb_append(sb, S("\n"));
		}

		if (tag == TAG_LI)
		{
		    for (i32 i = 0; i < listdepth - 1; i++)
			sb_append(sb, S("\t"));
		    sb_append(sb, liststyle);
		    sb_append(sb, S(" "));
		}

		append_definition(s, sb, liststyle, listdepth);
	    }
	    else
		json_skip(s);
	}
	else
	{
	    debug_msg("Encountered unknown type '%s' in structured content. Skipping..", json_typename[type]);
	    json_skip(s);
	}
    }

    /* if (list) frees8(&liststyle); */
    assert(type == JSON_OBJECT_END);
}

static void
append_definition(json_stream s[static 1], stringbuilder_s sb[static 1], s8 liststyle, i32 listdepth)
{
    enum json_type type = json_next(s);

    if (type == JSON_ARRAY)
    {
	type = json_next(s);

	while (type != JSON_ARRAY_END
	       && type != JSON_ERROR
	       && type != JSON_DONE)
	{
	    if (type == JSON_STRING)
		sb_append(sb, json_get_string_(s));
	    else if (type == JSON_OBJECT)
		append_structured_content(s, sb, liststyle, listdepth);
	    else
		debug_msg("Encountered unknown type '%s' within definition.", json_typename[type]);

	    type = json_next(s);
	}

	assert(type == JSON_ARRAY_END);
    }
    else if (type == JSON_STRING)
	sb_append(sb, json_get_string_(s));
    else if (type == JSON_OBJECT)
	append_structured_content(s, sb, liststyle, listdepth);
    else
	debug_msg("Encountered unknown type '%s' at start of definition.", json_typename[type]);
}

static void
freedictentry_(dictentry de[static 1])
{
    // dictname will be reused
    frees8(&de->kanji);
    frees8(&de->reading);
    frees8(&de->definition);
}

static void
add_dictionary(s8 buffer, s8 dictname)
{
    /* printf("Buffer: %.*s", (int)buffer.len, (char*)buffer.s); */
    json_stream s[1];
    json_open_buffer(s, buffer.s, buffer.len);

    enum json_type type;
    type = json_next(s);
    if (type != JSON_ARRAY)
    {
	error_msg("Dictionary format not supported.");
	return;
    }

    while(!sigint_received)
    {
	type = json_next(s);

	if (type == JSON_DONE)
	    break;
	else if (type == JSON_ERROR)
	{
	    error_msg("%s", json_get_error(s));
	    break;
	}
	else if (type == JSON_ARRAY)
	{
	    dictentry de = { .dictname = dictname };

	    /* first entry */
	    type = json_next(s);
	    assert(type == JSON_STRING);
	    de.kanji = s8dup(json_get_string_(s));
	    /* ----------- */

	    /* second entry */
	    type = json_next(s);
	    assert(type == JSON_STRING);
	    de.reading = s8dup(json_get_string_(s));
	    /* ----------- */

	    /* third entry */
	    type = json_next(s);
	    assert(type == JSON_STRING || type == JSON_NULL);
	    // Skip
	    /* ----------- */

	    /* fourth entry */
	    type = json_next(s);
	    assert(type == JSON_STRING);
	    // Skip
	    /* ----------- */

	    /* fifth entry */
	    type = json_next(s);
	    assert(type == JSON_NUMBER);
	    // Skip
	    /* ----------- */

	    /* sixth entry */
	    stringbuilder_s sb = sb_init(100);
	    append_definition(s, &sb, (s8){ 0 }, 0);
	    de.definition = sb_gets8(sb);
	    /* ----------- */

	    /* seventh entry */
	    type = json_next(s);
	    assert(type == JSON_NUMBER);
	    // Skip
	    /* ----------- */

	    /* eighth entry */
	    type = json_next(s);
	    assert(type == JSON_STRING);
	    // Skip
	    /* ----------- */

	    add_dictent_to_db(de);
	    freedictentry_(&de);
	    json_skip_until(s, JSON_ARRAY_END);
	}
    }

    json_close(s); // TODO: json_reset instead of close
}

static void
add_freq_to_db(s8 word, s8 reading, int freq)
{
    s8 data = concat(word, S("\0"), reading);
    addtodb3(data, freq);
    frees8(&data);
}

static void
add_frequency(s8 buffer)
{
    json_stream s[1];
    json_open_buffer(s, buffer.s, buffer.len);

    enum json_type type;
    type = json_next(s);
    assert(type == JSON_ARRAY);

    while(!sigint_received)
    {
	type = json_next(s);

	if (type == JSON_ARRAY_END || type == JSON_DONE)
	    break;
	else if (type == JSON_ERROR)
	{
	    error_msg("%s", json_get_error(s));
	    break;
	}
	else if (type == JSON_ARRAY)
	{
	    /* first entry */
	    type = json_next(s);
	    assert(type == JSON_STRING);
	    s8 word = s8dup(json_get_string_(s));
	    /* ----------- */

	    /* second entry */
	    type = json_next(s);
	    assert(type == JSON_STRING);
	    if (!s8equals(json_get_string_(s), S("freq")))
	    {
		debug_msg("Entry is not a frequency");
		json_skip_until(s, JSON_ARRAY); // Doesn't work with embedded arrays
		continue;
	    }
	    /* ----------- */

	    /* third entry */
	    s8 reading = { 0 };
	    int freq = -1;

	    type = json_next(s);
	    if (type == JSON_OBJECT)
	    {
		type = json_next(s);
		while (type != JSON_OBJECT_END && type != JSON_DONE && type != JSON_ERROR)
		{
		    if (type == JSON_STRING && s8equals(json_get_string_(s), S("reading")))
		    {
			type = json_next(s);
			assert(type == JSON_STRING);
			reading = s8dup(json_get_string_(s));
		    }
		    else if (type == JSON_STRING && s8equals(json_get_string_(s), S("frequency")))
		    {
			type = json_next(s);
			assert(type == JSON_NUMBER);
			freq = atoi(json_get_string(s, NULL)); // TODO: Error handling
		    }
		    else
			json_skip(s);

		    type = json_next(s);
		}

		assert(type == JSON_OBJECT_END);
	    }
	    else if (type == JSON_NUMBER)
		freq = atoi(json_get_string(s, NULL));     // TODO: Error handling
	    else
		assert(0);
	    /* ----------- */

	    add_freq_to_db(word, reading, freq);
	    frees8(&reading);
	    frees8(&word);

	    json_skip_until(s, JSON_ARRAY_END);
	}
	else
	    assert(0);
    }

    json_close(s);
}

s8
extract_dictname(zip_t* archive)
{
    s8 buffer = unzip_file(archive, "index.json");

    json_stream s[1];
    json_open_buffer(s, buffer.s, buffer.len);

    s8 dictname = { 0 };
    enum json_type type;
    for (;;)
    {
	type = json_next(s);
	s8 value;
	if (type == JSON_STRING)
	{
	    value = json_get_string_(s);
	    if (s8equals(value, S("title")))
	    {
		json_next(s);
		dictname = s8dup(json_get_string_(s));
		break;
	    }
	    else
		json_skip(s);
	}
    }

    json_close(s);
    frees8(&buffer);
    return dictname;
}

static zip_t*
open_zip(char* filename)
{
    int err = 0;
    zip_t* za;
    if ((za = zip_open(filename, 0, &err)) == NULL)
    {
	zip_error_t error;
	zip_error_init_with_code(&error, err);
	error_msg("Cannot open zip archive '%s': %s\n", filename, zip_error_strerror(&error));
	zip_error_fini(&error);
	return NULL;
    }

    return za;
}

static void
add_from_zip(char* filename)
{
    zip_t* za;
    if (!(za = open_zip(filename)))
	return;

    s8 dictname = extract_dictname(za);
    printf("Processing dictionary: %s\n", dictname.s);

    struct zip_stat finfo;
    zip_stat_init(&finfo);
    for (int i = 0; (zip_stat_index(za, i, 0, &finfo)) == 0 && !sigint_received; i++)
    {
	s8 fn = fromcstr_((char*)finfo.name); // Warning: Casts away const!
	if (startswith(fn, S("term_bank_")))
	{
	    s8 buffer = unzip_file(za, finfo.name); // Stats again, but anyway
	    add_dictionary(buffer, dictname);
	    frees8(&buffer);
	}
	else if (startswith(fn, S("term_meta_bank")))
	{
	    s8 buffer = unzip_file(za, finfo.name); // Stats again, but anyway
	    add_frequency(buffer);
	    frees8(&buffer);
	}
    }

    frees8(&dictname);
}

/*
 * Dialog which asks the user for confirmation (Yes/No)
 */
static bool
askyn(const char* msg)
{
    // TODO: Implement
    return true;
}

static void
check_file_existing(s8 dbpath)
{
    s8 data_fp = buildpath(dbpath, S("data.mdb"));
    if (access((char*)data_fp.s, R_OK) == 0)
    {
	if (askyn("A database file already exists. Would you like to delete the old one?")) // Maybe allow creating a backup
	    remove((char*)data_fp.s);
	else
	    exit(EXIT_FAILURE);
    }
    frees8(&data_fp);
}

static void
check_path_existing(s8 dbpath)
{
    char* dbpath_c = (char*)dbpath.s;
    if (access(dbpath_c, R_OK) != 0)
    {
	if (mkdir(dbpath_c, S_IRWXU | S_IRWXG | S_IXOTH))
	    fatal_perror("mkdir");
    }
}

void sigint_handler(int s)
{
    sigint_received = 1;
}

void setup_sighandler(void)
{
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);
}

int
main(int argc, char* argv[])
{
    setup_sighandler();
    int nextarg = parse_cmd_line_opts(argc, argv);

    s8 dbpath = argc - nextarg > 0 ? fromcstr_(argv[nextarg]) : get_default_path();
    debug_msg("Using database path: %s", dbpath.s);

    check_path_existing(dbpath);
    check_file_existing(dbpath);

    opendb((char*)dbpath.s);
    DIR *dir;
    if (!(dir = opendir(".")))
	fatal_perror("Opening current directory");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && !sigint_received)
    {
	s8 fn = fromcstr_(entry->d_name);
	if (endswith(fn, S(".zip")))
	    add_from_zip((char*)fn.s);
    }

    closedir(dir);
    closedb();
    s8 lockfile = buildpath(dbpath, S("lock.mdb"));
    remove((char*)lockfile.s);
    frees8(&lockfile);
}
