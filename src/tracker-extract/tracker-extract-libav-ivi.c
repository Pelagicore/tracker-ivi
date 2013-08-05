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

gchar *get_title_from_streams(AVFormatContext *ctx)
{
	AVDictionaryEntry *entry = NULL;
	int i = 0;
	for (;i < ctx->nb_streams; i++) {
		AVStream *s = ctx->streams[i];
		entry = av_dict_get(s->metadata, "title", NULL, 0);
		if (entry) {
			return entry->value;
		}
	}
	return NULL;
}

gchar *get_title_from_context(AVFormatContext *ctx)
{
	AVDictionaryEntry *entry = NULL;
	entry = av_dict_get(ctx->metadata, "title", NULL, 0);
	if (entry) {
		return entry->value;
	}
	return NULL;
}

gchar *get_title(const gchar *path)
{
	g_print("Path is: %s\n", path);
	AVFormatContext *ctx = NULL;
	char *title = NULL;
	int ret = 0;
	ret = avformat_open_input(&ctx, path, NULL, NULL);
	if (ret) {
		char err [1024];
		av_strerror(ret, err, 1024);
		printf("Error while opening file: %s\n", err);
		exit(0);
	}

	title = get_title_from_context(ctx);
	if (!title) {
		title = get_title_from_streams(ctx);
	}
	return title;
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

	file         = tracker_extract_info_get_file(info);
	filename     = g_file_get_path(file);
	uri          = g_file_get_uri(file);
	where        = g_string_new("");

	pre_builder  = tracker_extract_info_get_preupdate_builder(info);
	builder      = tracker_extract_info_get_metadata_builder(info);
	post_builder = tracker_extract_info_get_postupdate_builder(info);
	graph        = tracker_extract_info_get_graph(info);

	av_register_all();

	tracker_sparql_builder_predicate(builder, "a");
	tracker_sparql_builder_object(builder, "ivi:Video");

	if (title = get_title(filename)) {
		tracker_sparql_builder_predicate(builder, "ivi:videotitle");
		tracker_sparql_builder_object_unvalidated(builder, title);
	}

	tracker_extract_info_set_where_clause(info, where->str);

cleanup:
	g_string_free(where, TRUE);
	g_free(filename);
	if (error)
		g_error_free(error);
	return retval;
}
