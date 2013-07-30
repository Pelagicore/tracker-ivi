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

static gboolean
guess_dlna_profile (gint          width,
                    gint          height,
                    const gchar **dlna_profile,
                    const gchar **dlna_mimetype)
{
	const gchar *profile = NULL;

	if (dlna_profile) {
		*dlna_profile = NULL;
	}

	if (dlna_mimetype) {
		*dlna_mimetype = NULL;
	}

	if (width <= 640 && height <= 480) {
		profile = "JPEG_SM";
	} else if (width <= 1024 && height <= 768) {
		profile = "JPEG_MED";
	} else if (width <= 4096 && height <= 4096) {
		profile = "JPEG_LRG";
	}

	if (profile) {
		if (dlna_profile) {
			*dlna_profile = profile;
		}

		if (dlna_mimetype) {
			*dlna_mimetype = "image/jpeg";
		}

		return TRUE;
	}

	return FALSE;
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
	const gchar *dlna_profile, *dlna_mimetype, *graph;
	GPtrArray *keywords;
	gboolean success = TRUE;
	GString *where;
	guint i;

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
	md.orientation = tracker_coalesce_strip (3, xd->orientation, ed->orientation, id->image_orientation);
	md.copyright = tracker_coalesce_strip (4, xd->copyright, xd->rights, ed->copyright, id->copyright_notice);
	md.white_balance = tracker_coalesce_strip (2, xd->white_balance, ed->white_balance);
	md.fnumber = tracker_coalesce_strip (2, xd->fnumber, ed->fnumber);
	md.flash = tracker_coalesce_strip (2, xd->flash, ed->flash);
	md.focal_length =  tracker_coalesce_strip (2, xd->focal_length, ed->focal_length);
	md.artist = tracker_coalesce_strip (3, xd->artist, ed->artist, xd->contributor);
	md.exposure_time = tracker_coalesce_strip (2, xd->exposure_time, ed->exposure_time);
	md.iso_speed_ratings = tracker_coalesce_strip (2, xd->iso_speed_ratings, ed->iso_speed_ratings);
	md.date = tracker_coalesce_strip (5, xd->date, xd->time_original, ed->time, id->date_created, ed->time_original);
	md.description = tracker_coalesce_strip (2, xd->description, ed->description);
	md.metering_mode = tracker_coalesce_strip (2, xd->metering_mode, ed->metering_mode);
	md.city = tracker_coalesce_strip (2, xd->city, id->city);
	md.state = tracker_coalesce_strip (2, xd->state, id->state);
	md.address = tracker_coalesce_strip (2, xd->address, id->sublocation);
	md.country = tracker_coalesce_strip (2, xd->country, id->country_name);

	/* FIXME We are not handling the altitude ref here for xmp */
	md.gps_altitude = tracker_coalesce_strip (2, xd->gps_altitude, ed->gps_altitude);
	md.gps_latitude = tracker_coalesce_strip (2, xd->gps_latitude, ed->gps_latitude);
	md.gps_longitude = tracker_coalesce_strip (2, xd->gps_longitude, ed->gps_longitude);
	md.gps_direction = tracker_coalesce_strip (2, xd->gps_direction, ed->gps_direction);
	md.creator = tracker_coalesce_strip (3, xd->creator, id->byline, id->credit);
	md.comment = tracker_coalesce_strip (2, comment, ed->user_comment);
	md.make = tracker_coalesce_strip (2, xd->make, ed->make);
	md.model = tracker_coalesce_strip (2, xd->model, ed->model);

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

	if (md.make) {
		tracker_sparql_builder_predicate (metadata, "ivi:cameramanufacturer");
		tracker_sparql_builder_object_unvalidated (metadata, md.make);
	}
	if (md.model) {
		tracker_sparql_builder_predicate (metadata, "ivi:cameramodel");
		tracker_sparql_builder_object_unvalidated (metadata, md.model);
	}

	tracker_guarantee_title_from_file (metadata,
	                                   "ivi:imagetitle",
	                                   md.title,
	                                   uri,
	                                   NULL);

	if (md.orientation) {
		tracker_sparql_builder_predicate (metadata, "ivi:imageorientation");
		tracker_sparql_builder_object (metadata, md.orientation);
	}

	if (md.copyright) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagecopyright");
		tracker_sparql_builder_object_unvalidated (metadata, md.copyright);
	}
/*
	if (md.white_balance) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagewhitebalance");
		tracker_sparql_builder_object (metadata, md.white_balance);
	}
*/
	if (md.fnumber) {
		gdouble value;

		value = g_strtod (md.fnumber, NULL);
		tracker_sparql_builder_predicate (metadata, "ivi:imagefnumber");
		tracker_sparql_builder_object_double (metadata, value);
	}

	if (md.flash) {
		tracker_sparql_builder_predicate (metadata, "ivi:imageflash");
		tracker_sparql_builder_object (metadata, md.flash);
	}

	if (md.focal_length) {
		gdouble value;

		value = g_strtod (md.focal_length, NULL);
		tracker_sparql_builder_predicate (metadata, "ivi:imagefocallength");
		tracker_sparql_builder_object_double (metadata, value);
	}

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

		tracker_sparql_builder_predicate (metadata, "ivi:imagecreator");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (md.exposure_time) {
		gdouble value;

		value = g_strtod (md.exposure_time, NULL);
		tracker_sparql_builder_predicate (metadata, "ivi:imageexposuretime");
		tracker_sparql_builder_object_double (metadata, value);
	}

	if (md.iso_speed_ratings) {
		gdouble value;

		value = g_strtod (md.iso_speed_ratings, NULL);
		tracker_sparql_builder_predicate (metadata, "ivi:isospeed");
		tracker_sparql_builder_object_double (metadata, value);
	}

	tracker_guarantee_date_from_file_mtime (metadata,
	                                        "ivi:imagedate",
	                                        md.date,
	                                        uri);

	if (md.description) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagedescription");
		tracker_sparql_builder_object_unvalidated (metadata, md.description);
	}

	if (md.metering_mode) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagemeteringmode");
		tracker_sparql_builder_object (metadata, md.metering_mode);
	}

	if (md.creator && !md.artist) {
		gchar *uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", md.creator);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		if (graph) {
			tracker_sparql_builder_graph_open (preupdate, graph);
		}

		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "ivi:Artist");
		tracker_sparql_builder_predicate (preupdate, "ivi:artistname");
		tracker_sparql_builder_object_unvalidated (preupdate, md.creator);

		if (graph) {
			tracker_sparql_builder_graph_close (preupdate);
		}
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "ivi:imageartist");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (md.comment) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagecomment");
		tracker_sparql_builder_object_unvalidated (metadata, md.comment);
	}

	if (md.address || md.state || md.country || md.city ||
	    md.gps_altitude || md.gps_latitude || md.gps_longitude) {

		if (md.address || md.state || md.country || md.city) {
			if (md.address) {
				tracker_sparql_builder_predicate (metadata, "ivi:imageaddress");
				tracker_sparql_builder_object_unvalidated (metadata, md.address);
			}

			if (md.state) {
				tracker_sparql_builder_predicate (metadata, "ivi:imagestate");
				tracker_sparql_builder_object_unvalidated (metadata, md.state);
			}

			if (md.city) {
				tracker_sparql_builder_predicate (metadata, "ivi:imagecity");
				tracker_sparql_builder_object_unvalidated (metadata, md.city);
			}

			if (md.country) {
				tracker_sparql_builder_predicate (metadata, "ivi:imagecountry");
				tracker_sparql_builder_object_unvalidated (metadata, md.country);
			}
		}

		if (md.gps_altitude) {
			tracker_sparql_builder_predicate (metadata, "ivi:imagegpsaltitude");
			tracker_sparql_builder_object_unvalidated (metadata, md.gps_altitude);
		}

		if (md.gps_latitude) {
			tracker_sparql_builder_predicate (metadata, "ivi:imagegpslatitude");
			tracker_sparql_builder_object_unvalidated (metadata, md.gps_latitude);
		}

		if (md.gps_longitude) {
			tracker_sparql_builder_predicate (metadata, "ivi:imagegpslongitude");
			tracker_sparql_builder_object_unvalidated (metadata, md.gps_longitude);
		}
	}

	if (md.gps_direction) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagegpsdirection");
		tracker_sparql_builder_object_unvalidated (metadata, md.gps_direction);
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
