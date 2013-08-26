/*
 * Copyright (C) 2013, Pelagicore AB <jonatan.palsson@pelagicore.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <libtracker-extract/tracker-extract.h>
#include <libtracker-common/tracker-common.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>

gchar *get_property_from_streams(AVFormatContext *ctx, gchar *key)
{
	AVDictionaryEntry *entry = NULL;
	int i = 0;
	for (;i < ctx->nb_streams; i++) {
		AVStream *s = ctx->streams[i];
		entry = av_dict_get(s->metadata, key, NULL, 0);
		if (entry) {
			return entry->value;
		}
	}
	return NULL;
}

gchar *get_property_from_context(AVFormatContext *ctx, gchar *key)
{
	AVDictionaryEntry *entry = NULL;
	entry = av_dict_get(ctx->metadata, key, NULL, 0);
	if (entry) {
		return entry->value;
	}
	return NULL;
}

AVFormatContext *open_context(const gchar *path)
{
	g_print("Path is: %s\n", path);
	AVFormatContext *ctx = NULL;
	int ret = 0;
	ret = avformat_open_input(&ctx, path, NULL, NULL);
	if (ret) {
		char err [1024];
		av_strerror(ret, err, 1024);
		g_warning("Error while opening file: %s\n", err);
		if (ctx)
			avformat_free_context(ctx);
	}
	return ctx;
}

gchar *get_coalesced_property(const gchar *path,
                                    AVFormatContext *ctx,
                                    gchar *key,
                                    ...)
{
	gchar *i;
	va_list ap;
	va_start(ap, key);
	gchar *value = NULL;

	for (i = key; i != NULL; i = va_arg(ap, gchar*)) {
		value = get_property_from_streams(ctx, i);
		if (!value)
			value = get_property_from_context(ctx, i);
		if (value) break;
	}

	va_end(ap);
	return value;
}

gchar *get_title(const gchar *path, AVFormatContext *ctx)
{
	return get_coalesced_property(path, ctx, "title", NULL);
}

gchar *get_date(const gchar *path, AVFormatContext *ctx)
{
	return get_coalesced_property(path, ctx, "date",
	                                         "creation_time",
	                                         NULL);
}


G_MODULE_EXPORT gboolean
tracker_extract_get_metadata(TrackerExtractInfo *info)
{
	      GFile                *file;
	      gchar                *filename, *uri;
	      GString              *where;
	const gchar                *graph;
	      TrackerSparqlBuilder *builder, *pre_builder, *post_builder;
	      GError               *error = NULL;
	      gboolean              retval = TRUE;
	      gchar                *title = NULL;
	      gchar                *date = NULL;
	      AVFormatContext      *ctx = NULL;

	file         = tracker_extract_info_get_file(info);
	filename     = g_file_get_path(file);
	uri          = g_file_get_uri(file);
	where        = g_string_new("");

	pre_builder  = tracker_extract_info_get_preupdate_builder(info);
	builder      = tracker_extract_info_get_metadata_builder(info);
	post_builder = tracker_extract_info_get_postupdate_builder(info);
	graph        = tracker_extract_info_get_graph(info);

	av_register_all();

	ctx = open_context(filename);

	tracker_sparql_builder_predicate(builder, "a");
	tracker_sparql_builder_object(builder, "ivi:Video");

	if (title = get_title(filename, ctx)) {
		tracker_sparql_builder_predicate(builder, "ivi:videotitle");
		tracker_sparql_builder_object_unvalidated(builder, title);
	}

	tracker_sparql_builder_predicate (builder, "ivi:filecreated");
	if (date = get_date(filename, ctx)) {
		tracker_sparql_builder_object_unvalidated (builder,
		            date);
	} else {
		guint64 mtime;

		mtime = tracker_file_get_mtime_uri (uri);
		date = tracker_date_to_string ((time_t) mtime);
		tracker_sparql_builder_object_unvalidated (builder, date);
	}

	tracker_extract_info_set_where_clause(info, where->str);

cleanup:
	g_string_free(where, TRUE);
	g_free(date);
	g_free(filename);
	if (error)
		g_error_free(error);
	if (ctx)
		avformat_free_context(ctx);
	return retval;
}
