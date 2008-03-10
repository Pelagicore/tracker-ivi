/* Tracker Applet - tray icon for the tracker indexing daemon
 *
 * Copyright (C) 2007, Saleem Abdulrasool <compnerd@gentoo.org>
 * Copyright (C) 2007, Jamie McCracken <jamiemcc@blueyonder.co.uk>
 *
 * Portions derived from xscreensaver and gnome-screensaver
 * Copyright (c) 1991-2004 Jamie Zawinski <jwz@jwz.org>
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <X11/X.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <glade/glade.h>

#include "tracker-applet.h"
#include "tracker.h"
#include "tracker-applet-marshallers.h"

#define PROGRAM                 "tracker-applet"
#define PROGRAM_NAME            N_("Tracker Applet")

#define HOMEPAGE                "http://www.tracker-project.org/"
#define DESCRIPTION             "An applet for tracker"

#define DBUS_SERVICE_TRACKER    "org.freedesktop.Tracker"
#define DBUS_PATH_TRACKER       "/org/freedesktop/tracker"
#define DBUS_INTERFACE_TRACKER  "org.freedesktop.Tracker"

#define TRAY_ICON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), TYPE_TRAY_ICON, TrayIconPrivate))

#define TRACKER_ICON            "tracker-applet-default.png"
#define TRACKER_ICON_PAUSED     "tracker-applet-paused.png"
#define TRACKER_ICON_INDEX1     "tracker-applet-indexing1.png"
#define TRACKER_ICON_INDEX2     "tracker-applet-indexing2.png"


typedef enum {
	ICON_DEFAULT,
	ICON_PAUSED,
	ICON_INDEX1,
	ICON_INDEX2,
} IndexIcon;

typedef enum {
	INDEX_IDLE,
	INDEX_BUSY,
	INDEX_MERGING
} IndexStateEnum;


typedef enum {
	PAUSE_NONE,
	PAUSE_INTERNAL,
	PAUSE_BATTERY
} PauseStateEnum;


typedef enum {
	AUTO_PAUSE_NONE,
	AUTO_PAUSE_INDEXING,
	AUTO_PAUSE_MERGING
} AutoPauseEnum;


static char *index_icons[4] = {TRACKER_ICON, TRACKER_ICON_PAUSED, TRACKER_ICON_INDEX1, TRACKER_ICON_INDEX2};


typedef struct _TrayIconPrivate
{
   	GtkStatusIcon 		*icon;
   	GKeyFile		*keyfile;
   	char			*filename;

	/* settings */
	gboolean		auto_hide;
	gboolean		disabled;
	gboolean		show_animation;
	gboolean		reindex;
	AutoPauseEnum		auto_pause_setting;
	
	

	/* auto pause vars */
	gboolean		auto_pause_timer_active;
	time_t			auto_pause_last_time_event;

	gboolean		user_pause;
	gboolean		auto_pause;

	/* states */
	IndexStateEnum		index_state;
	PauseStateEnum		pause_state;
	IndexIcon		index_icon;
	gboolean		animated;
	gboolean		animated_timer_active;
	gboolean		is_watching_events;
	gboolean		email_indexing;
	gboolean		indexer_stopped;
		
	/* status hints */
	int			folders_indexed;
	int			folders_total;

	/* main window */
   	GtkMenu 		*menu;
	
	gboolean		initial_index_msg_shown;

	/* tracker connection */
	TrackerClient		*tracker;

	/* stats window table shown */
	gboolean		stat_window_active;
	gboolean		stat_request_pending;
	
	/* prefs window */
	GtkWidget 		*prefs_window;
	GtkWidget 		*chk_animate;
	GtkWidget 		*chk_show_icon;
	GtkWidget 		*opt_pause_off;	
	GtkWidget 		*opt_pause_index;
	GtkWidget 		*opt_pause_merge;
	GtkWidget		*btn_close;
	
} TrayIconPrivate;

static void tray_icon_class_init (TrayIconClass *klass);
static void tray_icon_init (GTypeInstance *instance, gpointer g_class);
static void tray_icon_clicked (GtkStatusIcon *icon, guint button, guint timestamp, gpointer data);
static void preferences_menu_activated (GtkMenuItem *item, gpointer data);
static void about_menu_activated (GtkMenuItem *item, gpointer data);
static void statistics_menu_activated (GtkMenuItem *item, gpointer data);
static void quit_menu_activated (GtkMenuItem *item, gpointer data);
static gboolean set_icon (TrayIconPrivate *priv);
static void set_auto_pause (TrayIcon *icon, gboolean pause);
static void create_prefs (TrayIcon *icon);

static TrayIcon *main_icon;

/* translatable strings */



static char *initial_index_1; 
static char *initial_index_2;

static char *end_index_initial_msg;
static char *end_index_hours_msg;
static char *end_index_minutes_msg;
static char *end_index_seconds_msg;
static char *end_index_final_msg;

typedef struct {
	char 		*name;
	char 		*label;
	GtkWidget 	*stat_label;
} Stat_Info;

static Stat_Info stat_info[13] = {

	{"Files",  		NULL,	NULL},
	{"Folders",  		NULL, 	NULL},
	{"Documents",  		NULL, 	NULL},
	{"Images",  		NULL, 	NULL},
	{"Music",  		NULL, 	NULL},
	{"Videos", 		NULL, 	NULL},
	{"Text",  		NULL, 	NULL},
	{"Development",  	NULL, 	NULL},
	{"Other",  		NULL, 	NULL},
	{"Applications",  	NULL, 	NULL},
	{"Conversations",  	NULL, 	NULL},
	{"Emails",  		NULL,	NULL},
	{ NULL, 		NULL,	NULL},
};


static void refresh_stats (TrayIcon *icon);
static inline void start_notice_events (Window     window);

static gboolean
query_pointer_timeout (Window window)
{
        Window       root;
        Window       child;
        int          root_x;
        int          root_y;
        int          win_x;
        int          win_y;
        unsigned int mask;

        gdk_error_trap_push ();
        XQueryPointer (GDK_DISPLAY (),
                       window,
                       &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);
        gdk_display_sync (gdk_display_get_default ());
        gdk_error_trap_pop ();

        return FALSE;
}

static void
set_status_hint (TrayIcon *icon)
{
	/* Tracker : indexing 10/120 folders */
	
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	const char *status;
	GString *hint;
	
	/* Translators: this will appear like "Tracker : Idle" */
	hint = g_string_new (_("Tracker : "));
	
	switch (priv->index_state) {
	
	case INDEX_IDLE:
		status =  _("Idle");
		break;
	
	case INDEX_BUSY:
		status = _("Indexing");
		break;
	
	case INDEX_MERGING:
		status = _("Merging");
		break;
			
	}
		
	
	g_string_append (hint, status);
	
		
	if (priv->user_pause) {
		status = _(" (paused by user)");
	} else if (priv->auto_pause) {
		status = _(" (paused by system)");
	
	} else {
					
		switch (priv->pause_state) {
	
		case PAUSE_NONE:
			status = g_strdup ("");
			break;
	
		case PAUSE_INTERNAL:
			status = _(" (paused by system)");
			break;
	
		case PAUSE_BATTERY:
			status = _(" (paused by battery)");
			break;
			
		}
	}

	g_string_append (hint, status);

		
	
	if (priv->index_state == INDEX_BUSY) {
	
		if (!priv->email_indexing) {
			status = _("folders");	
		} else {
			status = _("mailboxes");
		}
		
		g_string_append_printf (hint, " %d/%d %s", priv->folders_indexed, priv->folders_total, status);	

	}
	
	
	if (priv->index_state == INDEX_MERGING) {
		g_string_append_printf (hint, " %d/%d indexes being merged", priv->folders_indexed, priv->folders_total);	
	}

	tray_icon_set_tooltip (icon, hint->str);
	
	g_string_free (hint, TRUE);

}


static  gboolean
can_auto_pause (TrayIcon *icon)
{

	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	
	if (priv->user_pause || priv->pause_state == PAUSE_BATTERY || priv->disabled || priv->indexer_stopped) return FALSE;
	

	switch (priv->auto_pause_setting) {
	
	case AUTO_PAUSE_NONE:
		return FALSE;
	
	case AUTO_PAUSE_INDEXING:
		return (priv->index_state != INDEX_IDLE);
	
	case AUTO_PAUSE_MERGING:
		return (priv->index_state == INDEX_MERGING);		
		
	}
			
	return TRUE;

}

static gboolean
auto_pause_timeout (TrayIcon *icon)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
		
	time_t t = time (NULL);
	
	if (priv->indexer_stopped) return FALSE;
			
	if ((t >= (priv->auto_pause_last_time_event + 2))) {
		set_auto_pause (icon, FALSE);
		return FALSE;
	}
	
	dbus_g_proxy_begin_call (priv->tracker->proxy, "PromptIndexSignals", NULL, NULL, NULL, G_TYPE_INVALID);

        return TRUE;
}

static void
start_auto_pause_timer (TrayIcon *icon)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	if (!can_auto_pause (icon)) return;
	
	priv->auto_pause_last_time_event = time (NULL);
	
	if (!priv->auto_pause_timer_active) {
		g_timeout_add_seconds (2, (GSourceFunc)auto_pause_timeout, icon);
		set_auto_pause (icon, TRUE);
		
	}

}


static void
set_auto_pause (TrayIcon *icon, gboolean pause)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	GError *error = NULL; 
	
	/* do not pause/unpause if in user pause  */
	if (priv->user_pause) {
		priv->auto_pause_timer_active = FALSE;
		priv->auto_pause = FALSE;	
		return;
	}
	
	priv->auto_pause = pause;
	
	if (pause) {
			
		priv->auto_pause_last_time_event = time (NULL);
	
		if (!priv->auto_pause_timer_active) {
		
			g_timeout_add_seconds (2, (GSourceFunc)auto_pause_timeout, icon);
		
			priv->auto_pause_timer_active = TRUE;
					
			tracker_set_bool_option	(priv->tracker, "Pause", TRUE, &error);
					
		}
		
		priv->animated = FALSE;				
	
	
	} else {
	
		priv->auto_pause_timer_active = FALSE;
		priv->auto_pause = FALSE;	
		
		tracker_set_bool_option	(priv->tracker, "Pause", FALSE, &error);
		
	
	}
	
	set_icon (priv);

}


static void
set_user_pause (TrayIcon *icon, gboolean pause)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	GError *error = NULL;
	
	priv->user_pause = pause;
	
		
	if (pause) {
		tracker_set_bool_option	(priv->tracker, "Pause", TRUE, &error);
	
	} else {
		tracker_set_bool_option	(priv->tracker, "Pause", FALSE, &error);
	}
	
		
}


static GdkFilterReturn
filter_x_events (GdkXEvent *xevent,
		 GdkEvent  *event,
		 gpointer   data)
{
	XEvent *ev;

        ev = xevent;
        
	TrayIcon *icon = data;
	

      switch (ev->xany.type) {
        case KeyPress:
        case KeyRelease:
        case ButtonPress:
        case ButtonRelease:
		start_auto_pause_timer (icon);
                break;

        case PropertyNotify:
                if (ev->xproperty.atom == gdk_x11_get_xatom_by_name ("_NET_WM_USER_TIME")) {
			start_auto_pause_timer (icon);
                }
                break;
                
        case CreateNotify:
                {
                        Window window = ev->xcreatewindow.window;
                        start_notice_events (window);
                }
                break;
                
        case MotionNotify:
                if (ev->xmotion.is_hint) {
                        /* need to respond to hints so we continue to get events */
                        g_timeout_add (1000, (GSourceFunc)query_pointer_timeout, GINT_TO_POINTER (ev->xmotion.window));
                }

		start_auto_pause_timer (icon);
                break;
                
        default:
                break;
        }

	
	return GDK_FILTER_CONTINUE;


}


static void
notice_events_inner (Window   window,
                     gboolean enable,
                     gboolean top)
{
        XWindowAttributes attrs;
        unsigned long     events;
        Window            root;
        Window            parent;
        Window           *kids;
        unsigned int      nkids;
        int               status;
        GdkWindow        *gwindow;

        gwindow = gdk_window_lookup (window);
        if (gwindow != NULL
            && (window != GDK_ROOT_WINDOW ())) {
                /* If it's one of ours, don't mess up its event mask. */
                return;
        }

        kids = NULL;
        status = XQueryTree (GDK_DISPLAY (), window, &root, &parent, &kids, &nkids);

        if (status == 0) {
                if (kids != NULL) {
                        XFree (kids);
                }
                return;
        }

        if (window == root) {
                top = FALSE;
        }
        

        memset (&attrs, 0, sizeof (attrs));
        XGetWindowAttributes (GDK_DISPLAY (), window, &attrs);

        if (enable) {
                /* Select for KeyPress on all windows that already have it selected */
                events = ((attrs.all_event_masks | attrs.do_not_propagate_mask) & KeyPressMask);

                /* Keep already selected events.  This is important when the
                   window == GDK_ROOT_WINDOW () since the mask will contain
                   StructureNotifyMask that is essential for RANDR support */
                events |= attrs.your_event_mask;

                /* Select for SubstructureNotify on all windows */
                events |= SubstructureNotifyMask;

                /* Select for PropertyNotify events to get user time changes */
                events |= PropertyChangeMask;

                /* As with keypress events, only select mouse motion events
                   for windows which already have them selected. */
                events |= ((attrs.all_event_masks | attrs.do_not_propagate_mask) & (PointerMotionMask | PointerMotionHintMask));
        } else {
                /* We want to disable all events */

                /* Don't mess up the root window */
                if (window == GDK_ROOT_WINDOW ()) {
                        events = attrs.your_event_mask;
                } else {
                        events = 0;
                }
        }

        /* Select for SubstructureNotify on all windows.
           Select for KeyPress on all windows that already have it selected.

           Note that we can't select for ButtonPress, because of X braindamage:
           only one client at a time may select for ButtonPress on a given
           window, though any number can select for KeyPress.  Someone explain
           *that* to me.

           So, if the user spends a while clicking the mouse without ever moving
           the mouse or touching the keyboard, we won't know that they've been
           active, and the screensaver will come on.  That sucks, but I don't
           know how to get around it.

           Since X presents mouse wheels as clicks, this applies to those, too:
           scrolling through a document using only the mouse wheel doesn't
           count as activity...  Fortunately, /proc/interrupts helps, on
           systems that have it.  Oh, if it's a PS/2 mouse, not serial or USB.
           This sucks!
        */

        XSelectInput (GDK_DISPLAY (), window, events);

        if (top && (events & KeyPressMask)) {
                /* Only mention one window per tree */
                top = FALSE;
                
        }

        if (kids != NULL) {
                while (nkids > 0) {
                        notice_events_inner (kids [--nkids], enable, top);
                }

                XFree (kids);
        }
}

static void
notice_events (Window   window,
               gboolean enable)
               
{
        gdk_error_trap_push ();

        notice_events_inner (window, enable, TRUE);

        gdk_display_sync (gdk_display_get_default ());
        gdk_error_trap_pop ();
}

static inline void
stop_notice_events (Window     window)
{
        notice_events (window, FALSE);
}

static inline void
start_notice_events (Window     window)
{
        notice_events (window, TRUE);
}


static void
start_watching_events (TrayIcon *icon)
{

	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	if (priv->is_watching_events) return;

	gdk_window_add_filter (NULL, (GdkFilterFunc)filter_x_events, icon);
	start_notice_events (DefaultRootWindow (GDK_DISPLAY ()));
	priv->is_watching_events = TRUE;
}

static void
stop_watching_events (TrayIcon *icon)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	if (!priv->is_watching_events) return;

	stop_notice_events (DefaultRootWindow (GDK_DISPLAY ()));
        gdk_window_remove_filter (NULL, (GdkFilterFunc)filter_x_events, icon);
        priv->is_watching_events = FALSE;

}

static void
tray_icon_class_init (TrayIconClass *klass)
{
	g_type_class_add_private (klass, sizeof(TrayIconPrivate));
}

static void
activate_icon (GtkStatusIcon *icon, gpointer data)
{
	const gchar *command = "tracker-search-tool";

	if (!g_spawn_command_line_async(command, NULL))
		g_warning("Unable to execute command: %s", command);
}

static void
search_menu_activated (GtkMenuItem *item, gpointer data)
{
	activate_icon (NULL, NULL);
}


static void
pause_menu_toggled (GtkCheckMenuItem *item, gpointer data)
{
	TrayIcon *icon = TRAY_ICON (data);

	set_user_pause (icon, gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)));
}



static inline void
set_auto_pause_setting (TrayIcon *icon, AutoPauseEnum auto_pause)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	
	priv->auto_pause_setting = auto_pause;
}


static void
applet_preferences_menu_activated (GtkMenuItem *item, gpointer data)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (data);
	
	create_prefs (data);
		
	gtk_widget_show (priv->prefs_window);
}



static void
restart_tracker (GtkDialog *dialog, gint response, TrayIcon *icon)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));

	if (response == GTK_RESPONSE_YES) {

		g_print ("attempting to restart tracker\n");

		TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
		
		priv->reindex = TRUE;
		
		dbus_g_proxy_begin_call (priv->tracker->proxy,
					 "Shutdown",
					 NULL,
					 NULL,
					 NULL,
					 G_TYPE_BOOLEAN,
					 TRUE, G_TYPE_INVALID);
					 
	}
}


static void
reindex (GtkMenuItem *item, TrayIcon *icon)
{
	GtkWidget *dialog;
	gchar *primary;
	gchar *secondary;

	primary = g_strdup (_("Re-index your system?"));
	secondary = g_strdup (_("Indexing can take a long time. Are you sure you want to re-index?"));

					     
	dialog = gtk_message_dialog_new (NULL,
                                 	GTK_DIALOG_MODAL,
                                 	GTK_MESSAGE_WARNING,
                                 	GTK_BUTTONS_YES_NO,
                                 	primary);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  secondary);
	
	g_free (primary);
	g_free (secondary);

		
	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (restart_tracker), icon);
	gtk_widget_show (dialog);

}


static void
create_context_menu (TrayIcon *icon)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	GtkWidget *item = NULL, *image = NULL;
	priv->menu = (GtkMenu *)gtk_menu_new();

	item = (GtkWidget *)gtk_check_menu_item_new_with_mnemonic (_("_Pause All Indexing"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), FALSE);
	g_signal_connect (G_OBJECT (item), "toggled", G_CALLBACK (pause_menu_toggled), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);


	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic (_("_Search"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (search_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);
	
	
	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic (_("_Re-index"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (reindex), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);	
	
	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic (_("_Preferences"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (applet_preferences_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic (_("_Indexer Preferences"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (preferences_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic (_("S_tatistics"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_INFO, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (statistics_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic (_("_About"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_ABOUT, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (about_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	item = (GtkWidget *)gtk_image_menu_item_new_with_mnemonic (_("_Quit"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_QUIT, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (quit_menu_activated), icon);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);

	gtk_widget_show_all (GTK_WIDGET (priv->menu));
}


static void
set_tracker_icon (TrayIconPrivate *priv)
{
	char *path;
	const char *name;

	name = index_icons [priv->index_icon];

	path = g_build_filename (TRACKER_DATADIR "/tracker/icons", name, NULL);

	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		gtk_status_icon_set_from_file (priv->icon, path);
	}
	
	g_free (path);

}


static gboolean
set_icon (TrayIconPrivate *priv)
{
	
	if (!priv->user_pause) {
	
		if (priv->index_state == INDEX_IDLE) {
	
			priv->animated = FALSE;
			priv->animated_timer_active = FALSE;
			
		 	if (priv->index_icon != ICON_DEFAULT) {
				priv->index_icon = ICON_DEFAULT;
				set_tracker_icon (priv);
			}
			
			return FALSE;
		}
	
	} 
	
	
	if (priv->user_pause || priv->auto_pause || priv->pause_state != PAUSE_NONE) {
	
	
		if (priv->index_icon != ICON_PAUSED) {
			priv->index_icon = ICON_PAUSED;
			set_tracker_icon (priv);		
		}
		
		priv->animated = FALSE;
		priv->animated_timer_active = FALSE;
		return FALSE;

	}

	if (priv->index_state != INDEX_IDLE) {

		if (priv->index_icon == ICON_INDEX2 || !priv->show_animation) {
			priv->index_icon = ICON_DEFAULT;
		} else if (priv->index_icon != ICON_INDEX1) {
			priv->index_icon = ICON_INDEX1;
		} else {
			priv->index_icon = ICON_INDEX2;
		}

		set_tracker_icon (priv);

		return TRUE;
	}

	
	
	return FALSE;

}




static void
index_finished (DBusGProxy *proxy,  int time_taken, TrayIcon *icon)
{
	char *format;

	int hours = time_taken/3600;

	int minutes = (time_taken/60 - (hours * 60));

	int seconds = (time_taken - ((minutes * 60) + (hours * 3600)));

	if (hours > 0) {
		format = g_strdup_printf (end_index_hours_msg, hours, minutes);
	} else if (minutes > 0) {
		format = g_strdup_printf (end_index_minutes_msg, minutes, seconds);
	} else {
		format = g_strdup_printf (end_index_seconds_msg, seconds);		
	}

	tray_icon_show_message (icon, "%s%s\n\n%s", end_index_initial_msg, format, end_index_final_msg);
	g_free (format);
	
	stop_watching_events (icon); 

}


static void
index_state_changed (DBusGProxy *proxy, const gchar *state, gboolean initial_index, gboolean in_merge, gboolean is_manual_paused, gboolean is_battery_paused, gboolean is_io_paused, gboolean is_indexing_enabled, TrayIcon *icon)
{

	if (!state) return;

	

	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	gboolean paused = FALSE;
	
	priv->indexer_stopped = FALSE;	
	
	if (!is_indexing_enabled) {
		priv->disabled = TRUE;
		gtk_status_icon_set_visible (priv->icon, FALSE);
		return;
	} else {
		priv->disabled = FALSE;
		if (!priv->auto_hide) {
			gtk_status_icon_set_visible (priv->icon, TRUE);
		}
	}	
	
		
	if (!priv->initial_index_msg_shown && initial_index) {
		priv->initial_index_msg_shown = TRUE;
		g_usleep (100000);
		tray_icon_show_message (icon, "%s\n\n%s\n", initial_index_1, initial_index_2); 

	}

	priv->animated = FALSE;
	priv->pause_state = PAUSE_NONE;
	
	
	/* set pause states if applicable */
	if (is_manual_paused) {
		
		stop_watching_events (icon);
	
		if (!priv->auto_pause) {
			priv->user_pause = TRUE;	
		}
				
		paused = TRUE;		
				
	} else 	if (is_battery_paused) {
		
		priv->pause_state = PAUSE_BATTERY;
		paused = TRUE;
				
	} else 	if (is_io_paused) {
		paused = TRUE;	
		priv->pause_state = PAUSE_INTERNAL;

	} 


	if (in_merge) {
		
		priv->index_state = INDEX_MERGING;
		priv->animated = TRUE;
			

	} else 	if (strcasecmp (state, "Idle") == 0) {
	
		priv->index_state = INDEX_IDLE;
		
	} else { 
	
		priv->index_state = INDEX_BUSY;
		priv->animated = TRUE;
	
	}

	set_icon (priv);

	/* should we animate? */
	
	if (!paused &&  priv->animated && priv->show_animation) {

		if (!priv->animated_timer_active) {
			priv->animated_timer_active = TRUE;
			//g_timeout_add_seconds (2, (GSourceFunc) set_icon, priv);
		}
	}
	
	set_status_hint (icon);
	
	if (can_auto_pause (icon)) {
		start_watching_events (icon); 
	} else {
		stop_watching_events (icon); 
	}

}

static void
index_progress_changed (DBusGProxy *proxy, const gchar *service, const char *uri, int index_count, int folders_processed, int folders_total,  TrayIcon *icon)
{

	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	if (folders_processed > folders_total) folders_processed = folders_total;
	
	priv->folders_indexed = folders_processed;
	priv->folders_total = folders_total;
	
	priv->email_indexing = (strcmp (service, "Emails") == 0);
	
	set_status_hint (icon);
	
	set_icon (priv);

	/* update stat window if its active */
	refresh_stats (icon);

}


static void
init_settings (TrayIcon *icon)
{

	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->index_state = INDEX_IDLE;
	priv->pause_state = PAUSE_NONE;
	priv->auto_pause_setting = AUTO_PAUSE_MERGING;
	priv->index_icon = ICON_DEFAULT;
	priv->animated = FALSE;
	priv->animated_timer_active  = FALSE;
	priv->user_pause = FALSE;
	priv->auto_pause = FALSE;
	priv->auto_hide = FALSE;
	priv->disabled = FALSE;
	priv->show_animation = TRUE;
	priv->auto_pause_timer_active = FALSE;
	priv->is_watching_events = FALSE;
	priv->initial_index_msg_shown = FALSE;
	priv->stat_window_active = FALSE;
	priv->stat_request_pending = FALSE;
	priv->indexer_stopped = FALSE;
	
	set_tracker_icon (priv);
}

static void
name_owner_changed (DBusGProxy * proxy, const gchar * name,
		    const gchar * prev_owner, const gchar * new_owner,
		    gpointer data)
{

	
	if (!g_str_equal (name, DBUS_SERVICE_TRACKER)) return;

	if (g_str_equal (new_owner, "")) {

		TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (data);
		/* tracker has exited so reset status and make invisible until trackerd relaunched */
		index_state_changed (proxy, "Idle", FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, data);
		init_settings (data);
		gtk_status_icon_set_visible (priv->icon, FALSE);
		priv->indexer_stopped = TRUE;
		g_print ("tracker has exited (reindex = %d)\n", priv->reindex);

		
		if (priv->reindex) {
			priv->reindex = FALSE;
			const gchar *command = "trackerd";

			g_print ("restarting trackerd\n");
			g_usleep (1000000);
			if (!g_spawn_command_line_async (command, NULL))
				g_warning ("Unable to execute command: %s", command);
		
		}
		

	}
}



static gboolean
setup_dbus_connection (TrayIcon *icon)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->tracker = tracker_connect (FALSE);

	if (!priv->tracker) {
		g_print ("Could not initialise Tracker\n");
		exit (1);
	}


	/* set signal handlers */
	dbus_g_object_register_marshaller (tracker_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN,
 					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_INVALID);

	dbus_g_object_register_marshaller (tracker_VOID__STRING_STRING_INT_INT_INT,
 					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID);

   	dbus_g_proxy_add_signal (priv->tracker->proxy,
                           "IndexStateChange",
                           G_TYPE_STRING,
                           G_TYPE_BOOLEAN,
                           G_TYPE_BOOLEAN,
                           G_TYPE_BOOLEAN,
                           G_TYPE_BOOLEAN,
                           G_TYPE_BOOLEAN,
                           G_TYPE_BOOLEAN,
                           G_TYPE_INVALID);

	dbus_g_proxy_add_signal (priv->tracker->proxy,
                           "IndexProgress",
                           G_TYPE_STRING,
                           G_TYPE_STRING,
                           G_TYPE_INT,
                           G_TYPE_INT,
                           G_TYPE_INT,
                           G_TYPE_INVALID);

	dbus_g_proxy_add_signal (priv->tracker->proxy,
                           "IndexFinished",
                           G_TYPE_INT,
                           G_TYPE_INVALID);

   	dbus_g_proxy_connect_signal (priv->tracker->proxy,
				"IndexStateChange",
                               	G_CALLBACK (index_state_changed),
                               	icon,
                               	NULL);

   	dbus_g_proxy_connect_signal (priv->tracker->proxy,
				"IndexProgress",
                               	G_CALLBACK (index_progress_changed),
                               	icon,
                               	NULL);

   	dbus_g_proxy_connect_signal (priv->tracker->proxy,
				"IndexFinished",
                               	G_CALLBACK (index_finished),
                               	icon,
                               	NULL);

	DBusGConnection *connection;
	DBusGProxy *dbus_proxy;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
	dbus_proxy = dbus_g_proxy_new_for_name (connection,
						      DBUS_SERVICE_DBUS,
						      DBUS_PATH_DBUS,
						      DBUS_INTERFACE_DBUS);

	dbus_g_proxy_add_signal (dbus_proxy,
				 "NameOwnerChanged",
				 G_TYPE_STRING,
				 G_TYPE_STRING,
				 G_TYPE_STRING, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (dbus_proxy,
				     "NameOwnerChanged",
				     G_CALLBACK (name_owner_changed),
				     icon, NULL);


	/* prompt for updated signals */
	dbus_g_proxy_begin_call (priv->tracker->proxy, "PromptIndexSignals", NULL, NULL, NULL, G_TYPE_INVALID);

	return FALSE;
}


static void
tray_icon_init (GTypeInstance *instance, gpointer g_class)
{
	TrayIcon *icon = TRAY_ICON (instance);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	
	priv->icon = gtk_status_icon_new ();
	
	init_settings (icon);
	
   	priv->reindex = FALSE;	

	g_signal_connect(G_OBJECT(priv->icon), "activate", G_CALLBACK (activate_icon), instance);
	g_signal_connect(G_OBJECT(priv->icon), "popup-menu", G_CALLBACK (tray_icon_clicked), instance);

	
	/* build context menu */
	create_context_menu (icon);

	gtk_status_icon_set_visible (priv->icon, FALSE);

	setup_dbus_connection (icon);

}



void
tray_icon_set_tooltip (TrayIcon *icon, const gchar *format, ...)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	gchar *tooltip = NULL;
	va_list args;

	va_start(args, format);
	tooltip = g_strdup_vprintf(format, args);
	va_end(args);

	gtk_status_icon_set_tooltip (priv->icon, tooltip);

	g_free(tooltip);
}



void 
tray_icon_show_message (TrayIcon *icon, const char *message, ...)
{
	va_list args;
   	gchar *msg = NULL;
	NotifyNotification *notification = NULL;
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
   
   	va_start (args, message);
   	msg = g_strdup_vprintf (message, args);
   	va_end (args);
   	
   	if (priv->disabled) return;
   	
   	if (!priv->auto_hide && !gtk_status_icon_get_visible (priv->icon)) gtk_status_icon_set_visible (priv->icon, TRUE);
   	   	
	notification = notify_notification_new_with_status_icon ("Tracker", msg, NULL, priv->icon);

	notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);

	notify_notification_show (notification, NULL);

	g_object_unref (notification);

	g_free (msg);
	
}


static void
tray_icon_clicked (GtkStatusIcon *icon, guint button, guint timestamp, gpointer data)
{
	TrayIcon *self = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (self);


	gtk_menu_popup (GTK_MENU (priv->menu), NULL, NULL, gtk_status_icon_position_menu, icon, button, timestamp);
}



static void
preferences_menu_activated (GtkMenuItem *item, gpointer data)
{
	const gchar *command = "tracker-preferences";

	if (!g_spawn_command_line_async (command, NULL))
		g_warning ("Unable to execute command: %s", command);
}






static gchar *
get_stat_value (gchar ***stat_array, const gchar *stat) 
{
	gchar **array;
	gint i = 0;

	while (stat_array[i][0]) {

		array = stat_array[i];

		if (array[0] && strcasecmp (stat, array[0]) == 0) {
			return array[1];
		}

		i++;
	}

	return NULL;
}

static void
stat_window_free (GtkWidget *widget, gint arg ,gpointer data)
{
	TrayIcon *icon = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	priv->stat_window_active = FALSE;

	gtk_widget_destroy (widget);
}


static void
update_stats  (GPtrArray *array,
	       GError *error,
	       gpointer data)

{
	TrayIcon *icon = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	if (error) {
		g_warning ("an error has occured: %s",  error->message);
		g_error_free (error);
		priv->stat_request_pending = FALSE;
		return;
	}

	if (!array) {
                return;
        }

	guint i = array->len;
	

	if (i < 1 || !priv->stat_window_active) {
		g_ptr_array_free (array, TRUE);
		return;
	}

	gchar ***pdata = (gchar ***) array->pdata;
	
	for (i=0; i<12; i++) {
		 gtk_label_set_text  (GTK_LABEL (stat_info[i].stat_label), get_stat_value (pdata, stat_info[i].name));  
	}

	g_ptr_array_free (array, TRUE);

	priv->stat_request_pending = FALSE;

}



static void
refresh_stats (TrayIcon *icon)
{
	
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);

	if (!priv->stat_window_active || priv->stat_request_pending) {
		return;
	}

	priv->stat_request_pending = TRUE;
	
	tracker_get_stats_async (priv->tracker, (TrackerGPtrArrayReply) update_stats, icon);

}


static void
statistics_menu_activated (GtkMenuItem *item, gpointer data)
{
	TrayIcon *icon = TRAY_ICON (data);
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	int i;

	GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Statistics"),
                                                         NULL,
                                                         GTK_DIALOG_NO_SEPARATOR | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_STOCK_CLOSE,
                                                         GTK_RESPONSE_CLOSE,
                                                         NULL);

        gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
 	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
 	gtk_window_set_icon_name (GTK_WINDOW (dialog), "gtk-info");
 	gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
 	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	

        GtkWidget *table = gtk_table_new (13, 2, TRUE) ;
        gtk_table_set_row_spacings (GTK_TABLE (table), 4);
        gtk_table_set_col_spacings (GTK_TABLE (table), 65);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);

        GtkWidget *title_label = gtk_label_new (NULL);
        gtk_label_set_markup (GTK_LABEL (title_label), _("<span weight=\"bold\" size=\"larger\">Index statistics</span>"));
        gtk_misc_set_alignment (GTK_MISC (title_label), 0, 0);
        gtk_table_attach_defaults (GTK_TABLE (table), title_label, 0, 2, 0, 1) ;
         
	for (i=0; i<12; i++) {
         	                                                                        
               	GtkWidget *label_to_add = gtk_label_new (stat_info[i].label);		   
                              
                gtk_label_set_selectable (GTK_LABEL (label_to_add), TRUE);                              
       	        gtk_misc_set_alignment (GTK_MISC (label_to_add), 0, 0);                                 
       	        gtk_table_attach_defaults (GTK_TABLE (table), label_to_add, 0, 1, i+1, i+2); 

       	        stat_info[i].stat_label = gtk_label_new ("") ;                                        

       	        gtk_label_set_selectable (GTK_LABEL (stat_info[i].stat_label), TRUE);                               
       	        gtk_misc_set_alignment (GTK_MISC (stat_info[i].stat_label), 0, 0);                                  
       	        gtk_table_attach_defaults (GTK_TABLE (table), stat_info[i].stat_label, 1, 2,  i+1, i+2);   

	}

	priv->stat_window_active = TRUE;

	refresh_stats (icon);

        GtkWidget *dialog_hbox = gtk_hbox_new (FALSE, 12);
        GtkWidget *info_icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
        gtk_misc_set_alignment (GTK_MISC (info_icon), 0, 0);
        gtk_container_add (GTK_CONTAINER (dialog_hbox), info_icon);
        gtk_container_add (GTK_CONTAINER (dialog_hbox), table);

        gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), dialog_hbox);

	g_signal_connect (G_OBJECT (dialog),
	                  "response",
                          G_CALLBACK (stat_window_free), icon);

	gtk_widget_show_all (dialog);	
}

static void 
open_uri(GtkWindow *parent, const char *uri)
{
	GtkWidget *dialog;
	GdkScreen *screen;
	GError *error = NULL;
	gchar *cmdline;

	screen = gtk_window_get_screen (parent);

	cmdline = g_strconcat ("xdg-open ", uri, NULL);

	if (gdk_spawn_command_line_on_screen (screen, cmdline, &error) == FALSE) {
		dialog = gtk_message_dialog_new (parent, GTK_DIALOG_DESTROY_WITH_PARENT, 
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
						 error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_error_free (error);
	}

	g_free (cmdline);
}

static void 
about_url_hook(GtkAboutDialog *dialog, const gchar *url, gpointer data)
{
	open_uri (GTK_WINDOW (dialog), url);
}

static void 
about_email_hook(GtkAboutDialog *dialog, const gchar *email, gpointer data)
{
	gchar *uri;

	uri = g_strconcat ("mailto:", email, NULL);
	open_uri (GTK_WINDOW(dialog), uri);
	g_free (uri);
}

static void
about_menu_activated (GtkMenuItem *item, gpointer data)
{

	const gchar *authors[] = {
		"Jamie McCracken <jamiemcc at gnome.org>",
		"Saleem Abdulrasool <compnerd at compnerd.org>"
		"Laurent Aguerreche <laurent.aguerreche at free fr>",
		"Luca Ferretti <elle.uca@libero.it>",
		"Eugenio <me at eugesoftware com>",
		"Michael Biebl <mbiebl at gmail com>",
		"Edward Duffy <eduffy at gmail com>",
		"Gergan Penkov <gergan at gmail com>",
		"Deji Akingunola <dakingun gmail com>",
		"Julien <julienc psychologie-fr org>",
		"Tom <tpgww@onepost.net>",
		"Samuel Cormier-Iijima <sciyoshi at gmail com>",
		"Eskil Bylund <eskil at letterboxes org>",
		"Ulrik Mikaelsson <ulrik mikaelsson gmail com>",
		"tobutaz <tobutaz gmail com>",
		"Mikkel Kamstrup Erlandsen <mikkel kamstrup gmail com>",
		"Baptiste Mille-Mathias <baptiste.millemathias gmail com>",
		"Richard Quirk <quirky@zoom.co.uk>",
		"Marcus Fritzsch <fritschy at googlemail com>",
		"Jedy Wang <Jedy Wang at Sun COM>",
		"Anders Aagaard <aagaande at gmail com>",
		"Fabien VALLON <fabien at sonappart net>",
		"Jaime Frutos Morales <acidborg at gmail com>",
		"Christoph Laimburg <christoph laimburg at rolmail net>",
		NULL
	};
	const gchar *documenters[] = {
		NULL
	};
	const gchar *license[] = {
		N_("Tracker is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version."),
		N_("Tracker is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with Tracker; if not, write to the Free Software Foundation, Inc., "
		   "59 Temple Place, Suite 330, Boston, MA  02111-1307  USA")
	};
	gchar *license_trans;

	license_trans = g_strjoin ("\n\n", _(license[0]), _(license[1]),
					   _(license[2]), NULL);

	/* Make URLs and email clickable in about dialog */
	gtk_about_dialog_set_url_hook (about_url_hook, NULL, NULL);
	gtk_about_dialog_set_email_hook (about_email_hook, NULL, NULL);

	gtk_show_about_dialog (NULL,
			       "version", VERSION,
			       "comments", _("Tracker is a tool designed to extract info and metadata about your personal data so that it can be searched easily and quickly"),
			       "copyright", _("Copyright \xC2\xA9 2005-2008 "
					      "The Tracker authors"),
			       "license", license_trans,
			       "wrap-license", TRUE,
			       "authors", authors,
			       "documenters", documenters,
				/* Translators should localize the following string
				 * which will be displayed at the bottom of the about
				 * box to give credit to the translator(s).
				 */
			       "translator-credits", _("translator-credits"),
			       "logo-icon-name", "tracker",
			       "website", "http://www.tracker-project.org/",
			       "website-label", _("Tracker Web Site"),
			       NULL);

	g_free (license_trans);	
}

static void
quit_menu_activated (GtkMenuItem *item, gpointer data)
{
	gtk_main_quit();
}

GType
tray_icon_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (TrayIconClass),
			NULL,                                   /* base_init */
			NULL,                                   /* base_finalize */
			(GClassInitFunc) tray_icon_class_init,  /* class_init */
			NULL,                                   /* class_finalize */
			NULL,                                   /* class_data */
			sizeof (TrayIcon),
			0,                                      /* n_preallocs */
			tray_icon_init                          /* instance_init */
		};

		type = g_type_register_static (G_TYPE_OBJECT, "TrayIconType", &info, 0);
	}

	return type;
}


static void
chk_animate_toggled_cb (GtkToggleButton *check_button, gpointer user_data)  
{
	TrayIcon *icon = user_data;
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	
	priv->show_animation = gtk_toggle_button_get_active (check_button);
}

static void
chk_show_icon_toggled_cb (GtkToggleButton *check_button, gpointer user_data)  
{
	TrayIcon *icon = user_data;
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	
	if (gtk_toggle_button_get_active (check_button)) {
		priv->auto_hide = TRUE;
		gtk_status_icon_set_visible (priv->icon, FALSE);
	} else {
		priv->auto_hide = FALSE;
		if (!priv->disabled)  gtk_status_icon_set_visible (priv->icon, TRUE);
	}
	
}

static void
opt_pause_off_group_changed_cb (GtkToggleButton *check_button, gpointer user_data)  
{
	if (!gtk_toggle_button_get_active (check_button)) return;
	
	

	TrayIcon *icon = user_data;
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	
	const char *name = gtk_widget_get_name (GTK_WIDGET (check_button));

	if (g_str_equal (name, "opt_pause_off")) {
		priv->auto_pause_setting = AUTO_PAUSE_NONE;
		priv->auto_pause = FALSE;
		stop_watching_events (icon); 
		return;
	}
	
	if (g_str_equal (name, "opt_pause_index")) {

		priv->auto_pause_setting = AUTO_PAUSE_INDEXING;
		
		if (can_auto_pause (icon)) {
			start_watching_events (icon); 
		} else {
			stop_watching_events (icon); 
		}
		return;
	}


	if (g_str_equal (name, "opt_pause_merge")) {

		priv->auto_pause_setting = AUTO_PAUSE_MERGING;
		if (can_auto_pause (icon)) {
			start_watching_events (icon); 
		} else {
			stop_watching_events (icon); 
		}
		return;
	}
	
	
	
}




static void
load_options (TrayIcon *icon)
{
	GError *error = NULL;
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	
	if (!priv->keyfile) {
		priv->keyfile = g_key_file_new ();
	}
	
	if (!g_file_test (priv->filename, G_FILE_TEST_EXISTS)) {
	
	
	
		gchar *tracker_dir = g_build_filename (g_get_user_config_dir (), "/tracker", NULL);

		if (!g_file_test (tracker_dir, G_FILE_TEST_EXISTS)) {
			g_mkdir_with_parents (tracker_dir, 0700);
		}

		g_free (tracker_dir);
	
	
		char *contents = g_strconcat ("[Applet]\n",
					"AnimateWhenIndexing=true\n\n",
					"AutoHideIcon=false\n\n",
					"SmartPause=2\n", NULL);
					
		g_file_set_contents (priv->filename, contents, strlen (contents), NULL);
		g_free (contents);
					
		
	}
	
	if (!g_key_file_load_from_file (priv->keyfile, priv->filename, G_KEY_FILE_KEEP_COMMENTS, &error) || error) {
		if (error) g_error ("failed: g_key_file_load_from_file(): %s\n",  error->message);
		priv->show_animation = TRUE;
		priv->auto_hide = FALSE;
		priv->auto_pause_setting = AUTO_PAUSE_MERGING;
		
		return;
	}
	
	if (g_key_file_has_key (priv->keyfile, "Applet", "AnimateWhenIndexing", NULL)) {
		priv->show_animation = g_key_file_get_boolean (priv->keyfile, "Applet", "AnimateWhenIndexing", NULL);
	} else {
		priv->show_animation = TRUE;
	}
		
	if (g_key_file_has_key (priv->keyfile, "Applet", "AutoHideIcon", NULL)) {
		priv->auto_hide = g_key_file_get_boolean (priv->keyfile, "Applet", "AutoHideIcon", NULL);
	} else {
		priv->auto_hide = FALSE;
	}

	if (g_key_file_has_key (priv->keyfile, "Applet", "SmartPause", NULL)) {
		priv->auto_pause_setting = g_key_file_get_integer (priv->keyfile, "Applet", "SmartPause", NULL);
	} else {
		priv->auto_pause_setting = AUTO_PAUSE_MERGING;
	}
		
	switch (priv->auto_pause_setting) {
	
	case AUTO_PAUSE_NONE:
		
		priv->auto_pause_setting = AUTO_PAUSE_NONE;
		priv->auto_pause = FALSE;
		stop_watching_events (icon);
		
		break;
	
	case AUTO_PAUSE_INDEXING:
		
		if (can_auto_pause (icon)) {
			start_watching_events (icon); 
		} else {
			stop_watching_events (icon); 
		}
		
		break;
	
	case AUTO_PAUSE_MERGING:
				
		if (can_auto_pause (icon)) {
			start_watching_events (icon); 
		} else {
			stop_watching_events (icon); 
		}
		
		break;
		
	}

}


static void
save_options (TrayIcon *icon)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	
	g_key_file_set_boolean (priv->keyfile, "Applet", "AnimateWhenIndexing", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->chk_animate)));
	g_key_file_set_boolean (priv->keyfile, "Applet", "AutoHideIcon", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->chk_show_icon)));
	g_key_file_set_integer (priv->keyfile, "Applet", "SmartPause", priv->auto_pause_setting);
		
		
	GError *error = NULL;
	guint length = 0;	
	char *contents = g_key_file_to_data (priv->keyfile, &length, &error);

	if (error) {
		g_error ("failed: g_key_file_to_data(): %s\n",
			 error->message);
			 
			 return;
	}


	g_file_set_contents (priv->filename, contents, -1, NULL);

	g_free (contents);

}

static void
prefs_closed (GtkWidget *widget, gpointer data)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (data);
	
	save_options (data);
	
	gtk_widget_destroy (priv->prefs_window);
}

static void
create_prefs (TrayIcon *icon)
{
	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (icon);
	
	GladeXML *gxml = glade_xml_new (TRACKER_DATADIR "/tracker/tracker-applet-prefs.glade", NULL, NULL);

	if (gxml == NULL) {
		g_error ("Unable to find locate tracker-applet-prefs.glade");
		priv->prefs_window = NULL;
		return;
	}
	
	

	priv->prefs_window = glade_xml_get_widget (gxml, "wnd_prefs");
	gtk_widget_hide (priv->prefs_window);
	gtk_window_set_deletable (GTK_WINDOW (priv->prefs_window), FALSE);
	
	priv->chk_animate = glade_xml_get_widget (gxml, "chk_animate");
	priv->chk_show_icon = glade_xml_get_widget (gxml, "chk_show_icon");
	priv->opt_pause_off = glade_xml_get_widget (gxml, "opt_pause_off");
	priv->opt_pause_index = glade_xml_get_widget (gxml, "opt_pause_index");
	priv->opt_pause_merge = glade_xml_get_widget (gxml, "opt_pause_merge");
	priv->btn_close = glade_xml_get_widget (gxml, "btn_close");
		

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->chk_animate), priv->show_animation);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->chk_show_icon), priv->auto_hide);

	switch (priv->auto_pause_setting) {
	
	case AUTO_PAUSE_NONE:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->opt_pause_off), TRUE);
			
		break;
	
	case AUTO_PAUSE_INDEXING:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->opt_pause_index), TRUE);
		
		
		break;
	
	case AUTO_PAUSE_MERGING:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->opt_pause_merge), TRUE);
		
		
		
		break;
		
	}
	
	
	/* connect signal handlers */
   	
	g_signal_connect (GTK_TOGGLE_BUTTON (priv->chk_animate), "toggled", G_CALLBACK (chk_animate_toggled_cb), main_icon);
	g_signal_connect (GTK_TOGGLE_BUTTON (priv->chk_show_icon), "toggled", G_CALLBACK (chk_show_icon_toggled_cb), main_icon);
	g_signal_connect (GTK_TOGGLE_BUTTON (priv->opt_pause_off), "toggled", G_CALLBACK (opt_pause_off_group_changed_cb), main_icon);	
	g_signal_connect (GTK_TOGGLE_BUTTON (priv->opt_pause_index), "toggled", G_CALLBACK (opt_pause_off_group_changed_cb), main_icon);
	g_signal_connect (GTK_TOGGLE_BUTTON (priv->opt_pause_merge), "toggled", G_CALLBACK (opt_pause_off_group_changed_cb), main_icon);
	g_signal_connect (priv->btn_close, "clicked", G_CALLBACK (prefs_closed), main_icon);
	
	
}

int
main (int argc, char *argv[])
{

	bindtextdomain (GETTEXT_PACKAGE, TRACKER_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	if (!notify_is_initted () && !notify_init (PROGRAM_NAME)) {
      		g_warning ("failed: notify_init()\n");
      		return EXIT_FAILURE;
   	}
	gtk_window_set_default_icon_name ("tracker");

	g_set_application_name (_("Tracker"));


	/* set translatable strings here */
	
	initial_index_1 = _("Your computer is about to be indexed so you can perform fast searches of your files and emails");
	initial_index_2 = _("You can pause indexing at any time and configure index settings by right clicking here");

	end_index_initial_msg = _("Tracker has finished indexing your system");
	end_index_hours_msg = _(" in %d hours and %d minutes");
	end_index_minutes_msg = _(" in %d minutes and %d seconds");
	end_index_seconds_msg = _(" in %d seconds");
	end_index_final_msg = _("You can now perform searches by clicking here");


	stat_info[0].label = _("Files:");
	stat_info[1].label = _("    Folders:");
	stat_info[2].label = _("    Documents:");
	stat_info[3].label = _("    Images:");
	stat_info[4].label = _("    Music:");
	stat_info[5].label = _("    Videos:");
	stat_info[6].label = _("    Text:");
	stat_info[7].label = _("    Development:");
	stat_info[8].label = _("    Other:");
	stat_info[9].label = _("Applications:");
	stat_info[10].label = _("Conversations:");
	stat_info[11].label = _("Emails:");

   	main_icon = g_object_new (TYPE_TRAY_ICON, NULL);
   	TrayIconPrivate *priv = TRAY_ICON_GET_PRIVATE (main_icon);
  	
   	priv->keyfile = NULL;
   	
   	priv->filename = g_build_filename (g_strdup (g_get_user_config_dir ()), "/tracker/tracker-applet.cfg", NULL);
   	
   	load_options (main_icon);
   	
   	
   	gtk_main ();

   	notify_uninit();

   	return EXIT_SUCCESS;
}
