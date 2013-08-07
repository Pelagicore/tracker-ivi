/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include <libtracker-common/tracker-keyfile-object.h>

#include "tracker-db-config.h"

/* GKeyFile defines */
#define GROUP_JOURNAL     "Journal"

/* Default values */
#define DEFAULT_JOURNAL_CHUNK_SIZE           50
#define DEFAULT_JOURNAL_ROTATE_DESTINATION   ""

static void config_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec);
static void config_get_property (GObject      *object,
                                 guint         param_id,
                                 GValue       *value,
                                 GParamSpec   *pspec);
static void config_finalize     (GObject      *object);
static void config_constructed  (GObject      *object);

enum {
	PROP_0,

	/* Journal */
	PROP_JOURNAL_CHUNK_SIZE,
	PROP_JOURNAL_ROTATE_DESTINATION,
	PROP_USER_DATA_DIR,
	PROP_USER_CACHE_DIR
};

static TrackerConfigMigrationEntry migration[] = {
	{ G_TYPE_INT, GROUP_JOURNAL, "JournalChunkSize", "journal-chunk-size" },
	{ G_TYPE_STRING, GROUP_JOURNAL, "JournalRotateDestination", "journal-rotate-destination" },
};
static gchar *data_dir  = NULL;
static gchar *cache_dir = NULL;

G_DEFINE_TYPE (TrackerDBConfig, tracker_db_config, G_TYPE_SETTINGS);

static void
tracker_db_config_class_init (TrackerDBConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = config_set_property;
	object_class->get_property = config_get_property;
	object_class->finalize     = config_finalize;
	object_class->constructed  = config_constructed;

	g_object_class_install_property (object_class,
	                                 PROP_JOURNAL_CHUNK_SIZE,
	                                 g_param_spec_int ("journal-chunk-size",
	                                                   "Journal chunk size",
	                                                   " Size of the journal at rotation in MB. Use -1 to disable rotating",
	                                                   -1,
	                                                   G_MAXINT,
	                                                   DEFAULT_JOURNAL_CHUNK_SIZE,
	                                                   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_JOURNAL_ROTATE_DESTINATION,
	                                 g_param_spec_string ("journal-rotate-destination",
	                                                      "Journal rotate destination",
	                                                      " Destination to rotate journal chunks to",
	                                                      DEFAULT_JOURNAL_ROTATE_DESTINATION,
	                                                      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_USER_DATA_DIR,
	                                 g_param_spec_string ("user-data-dir",
	                                                      "User data dir",
	                                                      " Set to override the user data dir location, unset to use XDG_DATA_HOME",
	                                                      "",
	                                                      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
	                                 PROP_USER_CACHE_DIR,
	                                 g_param_spec_string ("user-cache-dir",
	                                                      "User cache dir",
	                                                      " Set to override the user cache dir location, unset to use XDG_CACHE_HOME",
	                                                      "",
	                                                      G_PARAM_READABLE));

}

static void
tracker_db_config_init (TrackerDBConfig *object)
{
}

static void
config_set_property (GObject      *object,
                     guint         param_id,
                     const GValue *value,
                     GParamSpec           *pspec)
{
	switch (param_id) {
		/* Journal */
	case PROP_JOURNAL_CHUNK_SIZE:
		tracker_db_config_set_journal_chunk_size (TRACKER_DB_CONFIG (object),
		                                          g_value_get_int(value));
		break;
	case PROP_JOURNAL_ROTATE_DESTINATION:
		tracker_db_config_set_journal_rotate_destination (TRACKER_DB_CONFIG (object),
		                                                  g_value_get_string(value));
		break;
	case PROP_USER_DATA_DIR:
		tracker_db_config_set_user_data_dir (TRACKER_DB_CONFIG (object),
		                                     g_value_get_string(value));
		break;
	case PROP_USER_CACHE_DIR:
		tracker_db_config_set_user_cache_dir (TRACKER_DB_CONFIG (object),
		                                     g_value_get_string(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_get_property (GObject    *object,
                     guint       param_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
	TrackerDBConfig *config = TRACKER_DB_CONFIG (object);

	switch (param_id) {
	case PROP_JOURNAL_CHUNK_SIZE:
		g_value_set_int (value, tracker_db_config_get_journal_chunk_size (config));
		break;
	case PROP_JOURNAL_ROTATE_DESTINATION:
		g_value_take_string (value, tracker_db_config_get_journal_rotate_destination (config));
		break;
	case PROP_USER_DATA_DIR:
		g_value_take_string (value, tracker_db_config_get_user_data_dir (config));
		break;
	case PROP_USER_CACHE_DIR:
		g_value_take_string (value, tracker_db_config_get_user_cache_dir (config));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
config_finalize (GObject *object)
{
	g_free (data_dir);
	g_free (cache_dir);
	(G_OBJECT_CLASS (tracker_db_config_parent_class)->finalize) (object);
}


static void
config_constructed (GObject *object)
{
	TrackerConfigFile *config_file;

	(G_OBJECT_CLASS (tracker_db_config_parent_class)->constructed) (object);

	g_settings_delay (G_SETTINGS (object));

	/* Migrate keyfile-based configuration */
	config_file = tracker_config_file_new ();
	if (config_file) {
		tracker_config_file_migrate (config_file,
		                             G_SETTINGS (object), migration);
		g_object_unref (config_file);
	}
}

TrackerDBConfig *
tracker_db_config_new (void)
{
	return g_object_new (TRACKER_TYPE_DB_CONFIG,
	                     "schema", "org.freedesktop.Tracker.DB",
	                     "path", "/org/freedesktop/tracker/db/",
	                     NULL);
}

/*
 * Get the data dir for the current user. First look in dconf, if there is no
 * value set here, or we don't have a valid TrackerDBConfig object, revert to
 * g_get_user_data_dir.
 */
gchar *
tracker_db_config_get_user_data_dir_safe (TrackerDBConfig *config)
{
	if (data_dir)
		return g_strdup(data_dir);

	if TRACKER_IS_DB_CONFIG (config)
		data_dir = tracker_db_config_get_user_data_dir (config);

	if (data_dir == NULL || g_strcmp0 (data_dir, "") == 0) {
		data_dir = g_build_filename (g_get_user_data_dir (),
		                             "tracker",
		                             "data",
		                             NULL);
		g_message ("Setting default data dir");
	} else {
		g_message ("Setting data dir based on user settings");
	}
	return g_strdup(data_dir);
}

/*
 * Get the cache dir for the current user. First look in dconf, if there is no
 * value set here, or we don't have a valid TrackerDBConfig object, revert to
 * g_get_user_cache_dir.
 */
gchar *
tracker_db_config_get_user_cache_dir_safe (TrackerDBConfig *config)
{
	if (cache_dir)
		return g_strdup(cache_dir);

	if TRACKER_IS_DB_CONFIG (config)
		cache_dir = tracker_db_config_get_user_cache_dir (config);

	if (cache_dir == NULL || g_strcmp0 (cache_dir, "") == 0) {
		cache_dir = g_build_filename (g_get_user_cache_dir (),
		                              "tracker",
		                              NULL);
		g_message ("Setting default cache dir");
	} else {
		g_message ("Setting cache dir based on user settings");
	}
	return cache_dir;
}

gboolean
tracker_db_config_save (TrackerDBConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_DB_CONFIG (config), FALSE);

	g_settings_apply (G_SETTINGS (config));

	return TRUE;
}


gint
tracker_db_config_get_journal_chunk_size (TrackerDBConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_DB_CONFIG (config), DEFAULT_JOURNAL_CHUNK_SIZE);

	return g_settings_get_int (G_SETTINGS (config), "journal-chunk-size");
}

gchar *
tracker_db_config_get_journal_rotate_destination (TrackerDBConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_DB_CONFIG (config), g_strdup (DEFAULT_JOURNAL_ROTATE_DESTINATION));

	return g_settings_get_string (G_SETTINGS (config), "journal-rotate-destination");
}

gchar *
tracker_db_config_get_user_data_dir (TrackerDBConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_DB_CONFIG (config), "");

	return g_settings_get_string (G_SETTINGS (config), "user-data-dir");
}

gchar *
tracker_db_config_get_user_cache_dir (TrackerDBConfig *config)
{
	g_return_val_if_fail (TRACKER_IS_DB_CONFIG (config), "");

	return g_settings_get_string (G_SETTINGS (config), "user-cache-dir");
}

void
tracker_db_config_set_journal_chunk_size (TrackerDBConfig *config,
                                          gint             value)
{
	g_return_if_fail (TRACKER_IS_DB_CONFIG (config));

	g_settings_set_int (G_SETTINGS (config), "journal-chunk-size", value);
	g_object_notify (G_OBJECT (config), "journal-chunk-size");
}

void
tracker_db_config_set_journal_rotate_destination (TrackerDBConfig *config,
                                                  const gchar     *value)
{
	g_return_if_fail (TRACKER_IS_DB_CONFIG (config));

	g_settings_set_string (G_SETTINGS (config), "journal-rotate-destination", value);
	g_object_notify (G_OBJECT (config), "journal-rotate-destination");
}

void
tracker_db_config_set_user_data_dir (TrackerDBConfig *config,
                                     const gchar     *value)
{
	g_return_if_fail (TRACKER_IS_DB_CONFIG (config));

	g_settings_set_string (G_SETTINGS (config), "user-data-dir", value);
	data_dir = g_strdup(value);
	g_object_notify (G_OBJECT (config), "user-data-dir");
}

void
tracker_db_config_set_user_cache_dir (TrackerDBConfig *config,
                                      const gchar     *value)
{
	g_return_if_fail (TRACKER_IS_DB_CONFIG (config));

	g_settings_set_string (G_SETTINGS (config), "user-cache-dir", value);
	cache_dir = g_strdup(value);
	g_object_notify (G_OBJECT (config), "user-cache-dir");
}
