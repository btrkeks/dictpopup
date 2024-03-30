#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "util.h"

static const u8 utf8_skip_data[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 1, 1
};
const u8* const utf8_skip = utf8_skip_data;

#define fatal_perror(context)     \
	do {                          \
	    perror(context);        \
	    abort();                \
	} while (0)

void* xmalloc(size_t nbytes)
{
    void* p = malloc(nbytes);
    if (!p)
    {
	perror("malloc");
	abort();
    }
    return p;
}

void* xcalloc(size_t num, size_t nbytes)
{
    void* p = calloc(num, nbytes);
    if (!p)
    {
	perror("calloc");
	abort();
    }
    return p;
}

void* xrealloc(void* ptr, size_t nbytes)
{
    void* p = realloc(ptr, nbytes);
    if (!p)
    {
	perror("realloc");
	abort();
    }
    return p;
}

/* --------------- Start s8 utils -------------- */
void
u8copy(u8* dst, u8* src, size n)
{
    assert(n >= 0);
    for (; n; n--)
    {
	*dst++ = *src++;
    }
}

i32
u8compare(u8* a, u8* b, size n)
{
    for (; n; n--)
    {
	i32 d = *a++ - *b++;
	if (d)
	    return d;
    }
    return 0;
}

/*
 * Copy src into dst returning the remaining portion of dst.
 */
s8
s8copy(s8 dst, s8 src)
{
    assert(dst.len >= src.len);

    u8copy(dst.s, src.s, src.len);
    dst.s += src.len;
    dst.len -= src.len;
    return dst;
}

s8
news8(size len)
{
    return (s8) {
	       .s = new(u8, len + 1), // Include NULL terminator
	       .len = len
    };
}

s8
s8dup(s8 s)
{
    s8 r = news8(s.len);
    u8copy(r.s, s.s, s.len);
    return r;
}

s8
fromcstr_(char* z)
{
    s8 s = { 0 };
    s.s = (u8 *)z;
    s.len = z ? strlen(z) : 0;
    return s;
}

i32
s8equals(s8 a, s8 b)
{
    return a.len == b.len && !u8compare(a.s, b.s, a.len);
}

s8
cuthead(s8 s, size off)
{
    assert(off >= 0);
    assert(off <= s.len);
    s.s += off;
    s.len -= off;
    return s;
}

s8
takehead(s8 s, size len)
{
    assert(len >= 0);
    assert(len <= s.len);
    s.len = len;
    return s;
}

s8
cuttail(s8 s, size len)
{
    assert(len >= 0);
    assert(len <= s.len);
    s.len -= len;
    return s;
}

s8
taketail(s8 s, size len)
{
    return cuthead(s, s.len - len);
}

b32
s8endswith(s8 s, s8 suffix)
{
    return s.len >= suffix.len && s8equals(taketail(s, suffix.len), suffix);
}

s8
s8striputf8chr(s8 s)
{
    // 0x80 = 10000000; 0xC0 = 11000000
    assert(s.len > 0);
    s.len--;
    while (s.len > 0 && (s.s[s.len] & 0x80) != 0x00 && (s.s[s.len] & 0xC0) != 0xC0)
	s.len--;
    return s;
}

s8
unescape(s8 str)
{
    size s = 0, e = 0;
    while (e < str.len)
    {
	if (str.s[e] == '\\' && e + 1 < str.len)
	{
	    switch (str.s[e + 1])
	    {
	    case 'n':
		str.s[s++] = '\n';
		e += 2;
		break;
	    case '\\':
		str.s[s++] = '\\';
		e += 2;
		break;
	    case '"':
		str.s[s++] = '"';
		e += 2;
		break;
	    case 'r':
		str.s[s++] = '\r';
		e += 2;
		break;
	    default:
		str.s[s++] = str.s[e++];
	    }
	}
	else
	    str.s[s++] = str.s[e++];
    }
    str.len = s;

    return str;
}

s8
concat_(s8 strings[static 1], const s8 stopper)
{
    size len = 0;
    for (s8* s = strings; !s8equals(*s, stopper); s++)
	len += s->len;

    s8 ret = news8(len);
    s8 p = ret;
    for (s8* s = strings; !s8equals(*s, stopper); s++)
	p = s8copy(p, *s);
    return ret;
}

s8
buildpath_(s8 pathcomps[static 1], const s8 stopper)
{
#ifdef _WIN32
    s8 sep = S("\\");
#else
    s8 sep = S("/");
#endif
    size pathlen = 0;
    bool first = true;
    for (s8* pc = pathcomps; !s8equals(*pc, stopper); pc++)
    {
	if (!first)
	    pathlen += sep.len;
	pathlen += pc->len;

	first = false;
    }

    s8 retpath = news8(pathlen);
    s8 p = retpath;

    first = true;
    for (s8* pc = pathcomps; !s8equals(*pc, stopper); pc++)
    {
	if (!first)
	    p = s8copy(p, sep);
	p = s8copy(p, *pc);

	first = false;
    }

    return retpath;
}

void
frees8(s8 z[static 1])
{
    free(z->s);
    *z = (s8){ 0 };
}

void
frees8buffer(s8* buf[static 1])
{
    while (buf_size(*buf) > 0)
	free(buf_pop(*buf).s);
    buf_free(*buf);
}

/* -------------- Start dictentry / dictionary utils ---------------- */

dictentry
dictentry_dup(dictentry de)
{
    return (dictentry){
	       .dictname = s8dup(de.dictname),
	       .kanji = s8dup(de.kanji),
	       .reading = s8dup(de.reading),
	       .definition = s8dup(de.definition)
    };
}

/* void */
/* dictentry_print(dictentry de) */
/* { */
/*     printf("dictname: %s\n" */
/* 	   "kanji: %s\n" */
/* 	   "reading: %s\n" */
/* 	   "definition: %s\n", */
/* 	   de.dictname, */
/* 	   de.kanji, */
/* 	   de.reading, */
/* 	   de.definition); */
/* } */

void
dictionary_add(dictentry* dict[static 1], dictentry de)
{
    buf_push(*dict, de);
}

size
dictlen(dictentry* dict)
{
    return buf_size(dict);
}

static void
dictentry_free(dictentry de[static 1])
{
    frees8(&de->dictname);
    frees8(&de->kanji);
    frees8(&de->reading);
    frees8(&de->definition);
}

void
dictionary_free(dictentry* dict[static 1])
{
    while (buf_size(*dict) > 0)
	dictentry_free(&buf_pop(*dict));
    buf_free(*dict);
}

dictentry
dictentry_at_index(dictentry* dict, size index)
{
    assert(index >= 0 && (size_t)index < buf_size(dict));
    return dict[index];
}
/* ------------------------------ */

stringbuilder_s
sb_init(size_t init_cap)
{
    return (stringbuilder_s) {
	       .len = 0,
	       .cap = init_cap,
	       .data = xcalloc(1, init_cap)
    };
}

void
sb_append(stringbuilder_s* b, s8 str)
{
    if (b->cap < b->len + str.len)
    {
	while (b->cap < b->len + str.len)
	{
	    b->cap *= 2;

	    if (b->cap < 0)
		abort();
	}

	b->data = xrealloc(b->data, b->cap);
	if (!b->data)
	    abort();
    }

    memcpy(b->data + b->len, str.s, str.len);
    b->len += str.len;
}

s8
sb_gets8(stringbuilder_s sb)
{
    return (s8) { .s = sb.data, .len = sb.len };
}

void
sb_free(stringbuilder_s* sb)
{
    free(sb->data);
    *sb = (stringbuilder_s) { 0 };
}
/* -------------- End dictentry / dictionary utils ---------------- */

static void
remove_substr(char str[static 1], s8 sub)
{
    char *s = str, *e = str;

    do{
	while (strncmp(e, (char*)sub.s, sub.len) == 0)
	{
	    e += sub.len;
	}
    } while ((*s++ = *e++));
}

s8
nuke_whitespace(s8 z)
{
    remove_substr((char*)z.s, S("\n"));
    remove_substr((char*)z.s, S("\t"));
    remove_substr((char*)z.s, S(" "));
    remove_substr((char*)z.s, S("　"));

    return fromcstr_((char*)z.s);
}