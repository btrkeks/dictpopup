#include "util.h"
/* #include <glib.h> */

#include <libnotify/notify.h>
void
notify(char* title, _Bool urgent, char const* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    char* txt = g_strdup_vprintf(fmt, argp);
    va_end(argp);
    if (!txt) return;

    notify_init("dictpopup");
    NotifyNotification* n = notify_notification_new(title, txt, NULL);
    notify_notification_set_urgency(n, urgent ? NOTIFY_URGENCY_CRITICAL : NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(n, 10000);

    GError* err = NULL;
    if (!notify_notification_show (n, &err)) {
	    fprintf (stderr, "Failed to send notification: %s\n", err->message);
	    g_error_free(err);
    }
    g_object_unref (G_OBJECT (n));
    g_free(txt);
}

#include "messages.h"

// Alternatively one could use: https://github.com/jtanx/libclipboard
#include <gtk/gtk.h>
s8
get_selection(void)
{
    gtk_init(NULL, NULL);
    GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    return fromcstr_(gtk_clipboard_wait_for_text(clipboard));
}

static s8
get_clipboard(void)
{
    gtk_init(NULL, NULL);
    GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    return fromcstr_(gtk_clipboard_wait_for_text(clipboard));
}


#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
/*
 * Wait until clipboard changes and return new clipboard contents.
 * Can return NULL.
 */
s8
get_sentence(void)
{
    Display* disp = XOpenDisplay(NULL);
    if (!disp)
	return (s8){0};
    Window root = DefaultRootWindow(disp);
    Atom clip = XInternAtom(disp, "CLIPBOARD", False);
    XFixesSelectSelectionInput(disp, root, clip,
			       XFixesSetSelectionOwnerNotifyMask);
    XEvent evt;
    XNextEvent(disp, &evt);     // Wait for clipboard to change
    XCloseDisplay(disp);

    return get_clipboard();
}

static char*
get_windowname_single(Display* dpy, Window win)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;     /* unused */
    unsigned char *wname = NULL;
    int status = !Success;

    Atom atoms[2] = {
	XInternAtom(dpy, "_NET_WM_NAME", False),
	XInternAtom(dpy, "WM_NAME", False)
    };

    for (size_t i = 0; i < sizeof(atoms) && status != Success; i++)
    {
	status = XGetWindowProperty(dpy, win, atoms[i], 0, (~0L),
				    False, AnyPropertyType, &actual_type,
				    &actual_format, &nitems, &bytes_after,
				    &wname);
    }

    return (char*)wname;
}

/*
 * Returns the window title of the currently active window
 * The return value can be freed using XFree.
 */
s8
get_windowname(void)
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy)
    {
	error_msg("ERROR: Can't open X display for retrieving the window title. Are you using Linux with X11?");
	return (s8){0};
    }

    Window foc_win;
    int revert_to;
    XGetInputFocus(dpy, &foc_win, &revert_to);

    char* wname = get_windowname_single(dpy, foc_win);

    // TODO: Need to iterate parent windows for this to work properly
    /* Window *windows; */
    /* int nwindows; */
    /* window_list(context, window_arg, &windows, &nwindows, False); */
    /* int w_index; */
    /* for (w_index = 0; w_index < nwindows; w_index++) */
    /* { */
    /* 	Window window = windows[w_index]; */
    /* 	{ */
    /* 	} */
    /* } */

    XCloseDisplay(dpy);
    return fromcstr_(wname);
}

void
play_audio(int len, char filepath[len])
{
    g_autofree char* cmd = g_strdup_printf("ffplay -nodisp -nostats -hide_banner -autoexit '%.*s'", len, filepath);

    GError* error = NULL;
    g_spawn_command_line_sync(cmd, NULL, NULL, NULL, &error);
    if (error)
    {
	error_msg("Failed to play file: %.*s. Error message: %s", len, filepath, error->message);
	g_error_free(error);
    }
}

/* char* */
/* get_user_data_dir(void) */
/* { */
/*     char* data_dir = NULL; */
/*     const char* data_dir_env = getenv("XDG_DATA_HOME"); */

/*     if (data_dir_env && *data_dir_env) */
/* 	data_dir = strdup(data_dir_env); */
/* #ifdef G_OS_WIN32 */
/*     else */
/* 	data_dir = get_special_folder(&FOLDERID_LocalAppData); */
/* #endif */
/*     if (!data_dir || !data_dir[0]) */
/*     { */
/* 	gchar *home_dir = g_build_home_dir(); */
/* 	g_free(data_dir); */
/* 	data_dir = g_build_filename(home_dir, ".local", "share", NULL); */
/* 	g_free(home_dir); */
/*     } */

/*     return data_dir; */
/* } */