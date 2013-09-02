/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <stdio.h>
#include <setjmp.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <jpeglib.h>

#include <libtracker-common/tracker-common.h>
#include <libtracker-extract/tracker-extract.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "tracker-main.h"

#define CM_TO_INCH              0.393700787

#ifdef HAVE_LIBEXIF
#define EXIF_NAMESPACE          "Exif"
#define EXIF_NAMESPACE_LENGTH   4
#endif /* HAVE_LIBEXIF */

#ifdef HAVE_EXEMPI
#define XMP_NAMESPACE           "http://ns.adobe.com/xap/1.0/\x00"
#define XMP_NAMESPACE_LENGTH    29
#endif /* HAVE_EXEMPI */

#ifdef HAVE_LIBIPTCDATA
#define PS3_NAMESPACE           "Photoshop 3.0\0"
#define PS3_NAMESPACE_LENGTH    14
#include <libiptcdata/iptc-jpeg.h>
#endif /* HAVE_LIBIPTCDATA */

typedef struct {
	const gchar *make;
	const gchar *model;
	const gchar *title;
	const gchar *orientation;
	const gchar *copyright;
	const gchar *white_balance;
	const gchar *fnumber;
	const gchar *flash;
	const gchar *focal_length;
	const gchar *artist;
	const gchar *exposure_time;
	const gchar *iso_speed_ratings;
	const gchar *date;
	const gchar *description;
	const gchar *metering_mode;
	const gchar *creator;
	const gchar *comment;
	const gchar *city;
	const gchar *state;
	const gchar *address;
	const gchar *country;
	const gchar *gps_altitude;
	const gchar *gps_latitude;
	const gchar *gps_longitude;
	const gchar *gps_direction;
} MergeData;

struct tej_error_mgr {
	struct jpeg_error_mgr jpeg;
	jmp_buf setjmp_buffer;
};

static void
extract_jpeg_error_exit (j_common_ptr cinfo)
{
	struct tej_error_mgr *h = (struct tej_error_mgr *) cinfo->err;
	(*cinfo->err->output_message)(cinfo);
	longjmp (h->setjmp_buffer, 1);
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	struct jpeg_decompress_struct cinfo;
	struct tej_error_mgr tejerr;
	struct jpeg_marker_struct *marker;
	TrackerSparqlBuilder *preupdate, *metadata;
	TrackerXmpData *xd = NULL;
	TrackerExifData *ed = NULL;
	TrackerIptcData *id = NULL;
	MergeData md = { 0 };
	GFile *file;
	FILE *f;
	goffset size;
	gchar *filename, *uri;
	gchar *comment = NULL;
	const gchar *graph;
	GPtrArray *keywords;
	gboolean success = TRUE;
	GString *where;

	metadata = tracker_extract_info_get_metadata_builder (info);
	preupdate = tracker_extract_info_get_preupdate_builder (info);
	graph = tracker_extract_info_get_graph (info);

	file = tracker_extract_info_get_file (info);
	filename = g_file_get_path (file);

	size = tracker_file_get_size (filename);

	if (size < 18) {
		g_free (filename);
		return FALSE;
	}

	f = tracker_file_open (filename);
	g_free (filename);

	if (!f) {
		return FALSE;
	}

	uri = g_file_get_uri (file);

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "ivi:Image");

	cinfo.err = jpeg_std_error (&tejerr.jpeg);
	tejerr.jpeg.error_exit = extract_jpeg_error_exit;
	if (setjmp (tejerr.setjmp_buffer)) {
		success = FALSE;
		goto fail;
	}

	jpeg_create_decompress (&cinfo);

	jpeg_save_markers (&cinfo, JPEG_COM, 0xFFFF);
	jpeg_save_markers (&cinfo, JPEG_APP0 + 1, 0xFFFF);
	jpeg_save_markers (&cinfo, JPEG_APP0 + 13, 0xFFFF);

	jpeg_stdio_src (&cinfo, f);

	jpeg_read_header (&cinfo, TRUE);

	/* FIXME? It is possible that there are markers after SOS,
	 * but there shouldn't be. Should we decompress the whole file?
	 *
	 * jpeg_start_decompress(&cinfo);
	 * jpeg_finish_decompress(&cinfo);
	 *
	 * jpeg_calc_output_dimensions(&cinfo);
	 */

	marker = (struct jpeg_marker_struct *) &cinfo.marker_list;

	while (marker) {
		gchar *str;
		gsize len;
#ifdef HAVE_LIBIPTCDATA
		gsize offset;
		guint sublen;
#endif /* HAVE_LIBIPTCDATA */

		switch (marker->marker) {
		case JPEG_COM:
			g_free (comment);
			comment = g_strndup ((gchar*) marker->data, marker->data_length);
			break;

		case JPEG_APP0 + 1:
			str = (gchar*) marker->data;
			len = marker->data_length;

#ifdef HAVE_LIBEXIF
			if (strncmp (EXIF_NAMESPACE, str, EXIF_NAMESPACE_LENGTH) == 0) {
				ed = tracker_exif_new ((guchar *) marker->data, len, uri);
			}
#endif /* HAVE_LIBEXIF */

#ifdef HAVE_EXEMPI
			if (strncmp (XMP_NAMESPACE, str, XMP_NAMESPACE_LENGTH) == 0) {
				xd = tracker_xmp_new (str + XMP_NAMESPACE_LENGTH,
				                      len - XMP_NAMESPACE_LENGTH,
				                      uri);
			}
#endif /* HAVE_EXEMPI */

			break;

		case JPEG_APP0 + 13:
			str = (gchar*) marker->data;
			len = marker->data_length;
#ifdef HAVE_LIBIPTCDATA
			if (len > 0 && strncmp (PS3_NAMESPACE, str, PS3_NAMESPACE_LENGTH) == 0) {
				offset = iptc_jpeg_ps3_find_iptc (str, len, &sublen);
				if (offset > 0 && sublen > 0) {
					id = tracker_iptc_new (str + offset, sublen, uri);
				}
			}
#endif /* HAVE_LIBIPTCDATA */

			break;

		default:
			marker = marker->next;
			continue;
		}

		marker = marker->next;
	}

	if (!ed) {
		ed = g_new0 (TrackerExifData, 1);
	}

	if (!xd) {
		xd = g_new0 (TrackerXmpData, 1);
	}

	if (!id) {
		id = g_new0 (TrackerIptcData, 1);
	}

	md.title = tracker_coalesce_strip (4, xd->title, ed->document_name, xd->title2, xd->pdf_title);
	md.copyright = tracker_coalesce_strip (4, xd->copyright, xd->rights, ed->copyright, id->copyright_notice);
	md.artist = tracker_coalesce_strip (3, xd->artist, ed->artist, xd->contributor);
	md.date = tracker_coalesce_strip (5, xd->date, xd->time_original, ed->time, id->date_created, ed->time_original);

	/* Prioritize on native dimention in all cases */
	tracker_sparql_builder_predicate (metadata, "ivi:imagewidth");
	tracker_sparql_builder_object_int64 (metadata, cinfo.image_width);

	/* TODO: add ontology and store ed->software */

	tracker_sparql_builder_predicate (metadata, "ivi:imageheight");
	tracker_sparql_builder_object_int64 (metadata, cinfo.image_height);

	keywords = g_ptr_array_new ();

	if (xd->keywords) {
		tracker_keywords_parse (keywords, xd->keywords);
	}

	if (xd->pdf_keywords) {
		tracker_keywords_parse (keywords, xd->pdf_keywords);
	}

	if (xd->subject) {
		tracker_keywords_parse (keywords, xd->subject);
	}

	where = g_string_new ("");
	tracker_extract_info_set_where_clause (info, where->str);
	g_string_free (where, TRUE);

	tracker_guarantee_title_from_file (metadata,
	                                   "ivi:imagetitle",
	                                   md.title,
	                                   uri,
	                                   NULL);

	if (md.artist) {
		gchar *uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", md.artist);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		if (graph) {
			tracker_sparql_builder_graph_open (preupdate, graph);
		}

		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "ivi:Artist");
		tracker_sparql_builder_predicate (preupdate, "ivi:artistname");
		tracker_sparql_builder_object_unvalidated (preupdate, md.artist);

		if (graph) {
			tracker_sparql_builder_graph_close (preupdate);
		}
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "ivi:imageartist");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	tracker_sparql_builder_predicate (metadata, "ivi:filecreated");
	if (metadata && md.date) {
		char *guessed_date = NULL;
		guessed_date = tracker_date_guess (md.date);
		if (guessed_date)
			tracker_sparql_builder_object_unvalidated (metadata,
		            guessed_date);
		else
			g_warning ("Invalid date!");
		g_free (guessed_date);
	} else {
		gchar *date;
		guint64 mtime;

		mtime = tracker_file_get_mtime_uri (uri);
		date = tracker_date_to_string ((time_t) mtime);
		tracker_sparql_builder_object_unvalidated (metadata, date);
		g_free(date);
	}

	tracker_sparql_builder_predicate (metadata, "ivi:filecreated");
	if (md.date) {
		tracker_sparql_builder_object_unvalidated (metadata,
		            md.date);
	} else {
		gchar *date;
		guint64 mtime;

		mtime = tracker_file_get_mtime_uri (uri);
		date = tracker_date_to_string ((time_t) mtime);
		tracker_sparql_builder_object_unvalidated (metadata, date);
		g_free(date);
	}

	jpeg_destroy_decompress (&cinfo);

	tracker_exif_free (ed);
	tracker_xmp_free (xd);
	tracker_iptc_free (id);
	g_free (comment);

fail:
	tracker_file_close (f, FALSE);
	g_free (uri);

	return success;
}
