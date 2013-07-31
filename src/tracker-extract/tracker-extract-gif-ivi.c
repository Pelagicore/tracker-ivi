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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gif_lib.h>

#include <libtracker-common/tracker-common.h>

#include <libtracker-extract/tracker-extract.h>

#define XMP_MAGIC_TRAILER_LENGTH 256
#define EXTENSION_RECORD_COMMENT_BLOCK_CODE 0xFE

typedef struct {
	const gchar *title;
	const gchar *date;
	const gchar *artist;
} MergeData;

typedef struct {
	gchar *width;
	gchar *height;
	gchar *comment;
} GifData;

typedef struct {
	unsigned int   byteCount;
	char          *bytes;
} ExtBlock;

static int
ext_block_append(ExtBlock *extBlock,
		 unsigned int len,
		 unsigned char extData[])
{
	extBlock->bytes = realloc(extBlock->bytes,extBlock->byteCount+len);
	if (extBlock->bytes == NULL) {
		return (GIF_ERROR);
	}

	memcpy(&(extBlock->bytes[extBlock->byteCount]), &extData[0], len);
	extBlock->byteCount += len;

	return (GIF_OK);
}

#if GIFLIB_MAJOR >= 5
static inline void
gif_error (const gchar *action, int err)
{
	const char *str = GifErrorString (err);
	if (str != NULL) {
		g_message ("%s, error: '%s'", action, str);
	} else {
		g_message ("%s, undefined error %d", action, err);
	}
}
#endif /* GIFLIB_MAJOR >= 5 */

static void
read_metadata (TrackerSparqlBuilder *preupdate,
               TrackerSparqlBuilder *metadata,
               GString              *where,
               GifFileType          *gifFile,
               const gchar          *uri,
               const gchar          *graph)
{
	GifRecordType RecordType;
	int frameheight;
	int framewidth;
	unsigned char *framedata = NULL;
	GPtrArray *keywords;
	guint i;
	int status;
	MergeData md = { 0 };
	GifData   gd = { 0 };
	TrackerXmpData *xd = NULL;

	do {
		GifByteType *ExtData;
		int ExtCode;
		ExtBlock extBlock;

		if (DGifGetRecordType(gifFile, &RecordType) == GIF_ERROR) {
#if GIFLIB_MAJOR < 5
			PrintGifError ();
#else  /* GIFLIB_MAJOR < 5 */
			gif_error ("Could not read next GIF record type", gifFile->Error);
#endif /* GIFLIB_MAJOR < 5 */
			return;
		}

		switch (RecordType) {
			case IMAGE_DESC_RECORD_TYPE:
			if (DGifGetImageDesc(gifFile) == GIF_ERROR) {
#if GIFLIB_MAJOR < 5
				PrintGifError();
#else  /* GIFLIB_MAJOR < 5 */
				gif_error ("Could not get GIF record information", gifFile->Error);
#endif /* GIFLIB_MAJOR < 5 */
				return;
			}

			framewidth  = gifFile->Image.Width;
			frameheight = gifFile->Image.Height;

			framedata = g_malloc (framewidth*frameheight);

			if (DGifGetLine(gifFile, framedata, framewidth*frameheight)==GIF_ERROR) {
#if GIFLIB_MAJOR < 5
				PrintGifError();
#else  /* GIFLIB_MAJOR < 5 */
				gif_error ("Could not load a block of GIF pixes", gifFile->Error);
#endif /* GIFLIB_MAJOR < 5 */
				return;
			}

			gd.width  = g_strdup_printf ("%d", framewidth);
			gd.height = g_strdup_printf ("%d", frameheight);


			g_free (framedata);

		break;
		case EXTENSION_RECORD_TYPE:
			extBlock.bytes = NULL;
			extBlock.byteCount = 0;

			if ((status = DGifGetExtension (gifFile, &ExtCode, &ExtData)) != GIF_OK) {
				g_warning ("Problem getting the extension");
				return;
			}
#if defined(HAVE_EXEMPI)
			if (ExtData && *ExtData &&
			    strncmp (&ExtData[1],"XMP Data",8) == 0) {
				while (ExtData != NULL && status == GIF_OK ) {
					if ((status = DGifGetExtensionNext (gifFile, &ExtData)) == GIF_OK) {
						if (ExtData != NULL) {
							if (ext_block_append (&extBlock, ExtData[0]+1, (char *) &(ExtData[0])) != GIF_OK) {
								g_warning ("Problem with extension data");
								return;
							}
						}
					}
				}

				xd = tracker_xmp_new (extBlock.bytes,
				                      extBlock.byteCount-XMP_MAGIC_TRAILER_LENGTH,
				                      uri);

				g_free (extBlock.bytes);
			} else
#endif
			/* See Section 24. Comment Extension. in the GIF format definition */
			if (ExtCode == EXTENSION_RECORD_COMMENT_BLOCK_CODE &&
			    ExtData && *ExtData) {
				guint block_count = 0;

				/* Merge all blocks */
				do {
					block_count++;

					g_debug ("Comment Extension block found (#%u, %u bytes)",
					         block_count,
					         ExtData[0]);
					if (ext_block_append (&extBlock, ExtData[0], (char *) &(ExtData[1])) != GIF_OK) {
						g_warning ("Problem with Comment extension data");
						return;
					}
				} while (((status = DGifGetExtensionNext(gifFile, &ExtData)) == GIF_OK) &&
				         ExtData != NULL);

				/* Add last NUL byte */
				g_debug ("Comment Extension blocks found (%u) with %u bytes",
				         block_count,
				         extBlock.byteCount);
				extBlock.bytes = g_realloc (extBlock.bytes, extBlock.byteCount + 1);
				extBlock.bytes[extBlock.byteCount] = '\0';

				/* Set commentt */
				gd.comment = extBlock.bytes;
			} else {
				do {
					status = DGifGetExtensionNext(gifFile, &ExtData);
				} while ( status == GIF_OK && ExtData != NULL);
			}
		break;
		case TERMINATE_RECORD_TYPE:
			break;
		default:
			break;
		}
	} while (RecordType != TERMINATE_RECORD_TYPE);


	if (!xd) {
		xd = g_new0 (TrackerXmpData, 1);
	}

	md.title = tracker_coalesce_strip (3, xd->title, xd->title2, xd->pdf_title);
	md.date = tracker_coalesce_strip (2, xd->date, xd->time_original);
	md.artist = tracker_coalesce_strip (2, xd->artist, xd->contributor);

	tracker_guarantee_date_from_file_mtime (metadata,
	                                        "ivi:imagedate",
	                                        md.date,
	                                        uri);

	if (xd->description) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagedescription");
		tracker_sparql_builder_object_unvalidated (metadata, xd->description);
	}

	if (xd->copyright) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagecopyright");
		tracker_sparql_builder_object_unvalidated (metadata, xd->copyright);
	}

	if (xd->make) {
		tracker_sparql_builder_predicate (metadata, "ivi:cameramanufacturer");
		tracker_sparql_builder_object_unvalidated (metadata, xd->make);
	}
	if (xd->model) {
		tracker_sparql_builder_predicate (metadata, "ivi:cameramodel");
		tracker_sparql_builder_object_unvalidated (metadata, xd->model);
	}

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

		tracker_sparql_builder_predicate (metadata, "ivi:artistname");
		tracker_sparql_builder_object_iri (metadata, uri);
		g_free (uri);
	}

	if (xd->orientation) {
		tracker_sparql_builder_predicate (metadata, "ivi:imageorientation");
		tracker_sparql_builder_object_unvalidated (metadata, xd->orientation);
	}

	if (xd->exposure_time) {
		tracker_sparql_builder_predicate (metadata, "ivi:imageexposuretime");
		tracker_sparql_builder_object_unvalidated (metadata, xd->exposure_time);
	}

	if (xd->iso_speed_ratings) {
		tracker_sparql_builder_predicate (metadata, "ivi:imageiso");
		tracker_sparql_builder_object_unvalidated (metadata, xd->iso_speed_ratings);
	}

	if (xd->white_balance) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagewhitebalance");
		tracker_sparql_builder_object_unvalidated (metadata, xd->white_balance);
	}

	if (xd->fnumber) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagefnumber");
		tracker_sparql_builder_object_unvalidated (metadata, xd->fnumber);
	}

	if (xd->flash) {
		tracker_sparql_builder_predicate (metadata, "ivi:imageflash");
		tracker_sparql_builder_object_unvalidated (metadata, xd->flash);
	}

	if (xd->focal_length) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagefocallength");
		tracker_sparql_builder_object_unvalidated (metadata, xd->focal_length);
	}

	if (xd->metering_mode) {
		tracker_sparql_builder_predicate (metadata, "ivi:meteringmode");
		tracker_sparql_builder_object_unvalidated (metadata, xd->metering_mode);
	}

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

	g_ptr_array_free (keywords, TRUE);

	if (xd->address) {
		tracker_sparql_builder_predicate (metadata, "ivi:imageaddress");
		tracker_sparql_builder_object_unvalidated (metadata, xd->address);
	}

	if (xd->state) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagestate");
		tracker_sparql_builder_object_unvalidated (metadata, xd->state);
	}

	if (xd->city) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagecity");
		tracker_sparql_builder_object_unvalidated (metadata, xd->city);
	}

	if (xd->country) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagecountry");
		tracker_sparql_builder_object_unvalidated (metadata, xd->country);
	}

	if (xd->gps_altitude) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagegpsaltitude");
		tracker_sparql_builder_object_unvalidated (metadata, xd->gps_altitude);
	}

	if (xd->gps_latitude) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagegpslatitude");
		tracker_sparql_builder_object_unvalidated (metadata, xd->gps_latitude);
	}

	if (xd->gps_longitude) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagegpslongitude");
		tracker_sparql_builder_object_unvalidated (metadata, xd->gps_longitude);
	}

	if (xd->gps_direction) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagegpsdirection");
		tracker_sparql_builder_object_unvalidated (metadata, xd->gps_direction);
	}

	if (gd.width) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagewidth");
		tracker_sparql_builder_object_unvalidated (metadata, gd.width);
		g_free (gd.width);
	}

	if (gd.height) {
		tracker_sparql_builder_predicate (metadata, "ivi:imageheight");
		tracker_sparql_builder_object_unvalidated (metadata, gd.height);
		g_free (gd.height);
	}

	if (gd.comment) {
		tracker_sparql_builder_predicate (metadata, "ivi:imagecomment");
		tracker_sparql_builder_object_unvalidated (metadata, gd.comment);
		g_free (gd.comment);
	}

	tracker_xmp_free (xd);
}


G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerSparqlBuilder *preupdate, *metadata;
	goffset size;
	GifFileType *gifFile = NULL;
	GString *where;
	const gchar *graph;
	gchar *filename, *uri;
	GFile *file;
	int fd;
#if GIFLIB_MAJOR >= 5
	int err;
#endif

	preupdate = tracker_extract_info_get_preupdate_builder (info);
	metadata = tracker_extract_info_get_metadata_builder (info);
	graph = tracker_extract_info_get_graph (info);

	file = tracker_extract_info_get_file (info);
	filename = g_file_get_path (file);
	size = tracker_file_get_size (filename);

	if (size < 64) {
		g_free (filename);
		return FALSE;
	}

	fd = tracker_file_open_fd (filename);

	if (fd == -1) {
		g_warning ("Could not open GIF file '%s': %s\n",
		           filename,
		           g_strerror (errno));
		g_free (filename);
		return FALSE;
	}	

#if GIFLIB_MAJOR < 5
	if ((gifFile = DGifOpenFileHandle (fd)) == NULL) {
		PrintGifError ();
#else   /* GIFLIB_MAJOR < 5 */
	if ((gifFile = DGifOpenFileHandle (fd, &err)) == NULL) {
		gif_error ("Could not open GIF file with handle", err);
#endif /* GIFLIB_MAJOR < 5 */
		close (fd);
		return FALSE;
	}

	g_free (filename);

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "ivi:Image");

	where = g_string_new ("");
	uri = g_file_get_uri (file);

	read_metadata (preupdate, metadata, where, gifFile, uri, graph);
	tracker_extract_info_set_where_clause (info, where->str);
	g_string_free (where, TRUE);

	g_free (uri);

	if (DGifCloseFile (gifFile) != GIF_OK) {
#if GIFLIB_MAJOR < 5
		PrintGifError ();
#else  /* GIFLIB_MAJOR < 5 */
		gif_error ("Could not close GIF file", gifFile->Error);
#endif /* GIFLIB_MAJOR < 5 */
	}

	return TRUE;
}
