/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
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
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#include <glib.h>

#include <vorbis/vorbisfile.h>

#include <libtracker-common/tracker-common.h>

#include <libtracker-extract/tracker-extract.h>

typedef struct {
	const gchar *creator;
	gchar *creator_uri;
} MergeData;

typedef struct {
	gchar *title;
	gchar *artist;
	gchar *album;
	gchar *album_artist;
	gchar *track_count;
	gchar *track_number;
	gchar *disc_number;
	gchar *performer;
	gchar *track_gain; 
	gchar *track_peak_gain;
	gchar *album_gain;
	gchar *album_peak_gain;
	gchar *date; 
	gchar *comment;
	gchar *genre; 
	gchar *codec;
	gchar *codec_version;
	gchar *sample_rate;
	gchar *channels; 
	gchar *mb_album_id; 
	gchar *mb_artist_id; 
	gchar *mb_album_artist_id;
	gchar *mb_track_id; 
	gchar *lyrics; 
	gchar *copyright; 
	gchar *license; 
	gchar *organization; 
	gchar *location;
	gchar *publisher;
} VorbisData;

static gchar *
ogg_get_comment (vorbis_comment *vc,
                 const gchar    *label)
{
	gchar *tag;
	gchar *utf_tag;

	if (vc && (tag = vorbis_comment_query (vc, label, 0)) != NULL) {
		utf_tag = g_locale_to_utf8 (tag, -1, NULL, NULL, NULL);
		/*g_free (tag);*/

		return utf_tag;
	} else {
		return NULL;
	}
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerSparqlBuilder *preupdate, *metadata;
	VorbisData vd = { 0 };
	MergeData md = { 0 };
	FILE *f;
	gchar *filename;
	OggVorbis_File vf;
	vorbis_comment *comment;
	vorbis_info *vi;
	unsigned int bitrate;
	gint time;
	GFile *file;
	const gchar *graph;
	gchar *file_uri = NULL;

	file = tracker_extract_info_get_file (info);
	filename = g_file_get_path (file);
	file_uri = g_file_get_uri (file);
	f = tracker_file_open (filename);
	g_free (filename);

	preupdate = tracker_extract_info_get_preupdate_builder (info);
	metadata = tracker_extract_info_get_metadata_builder (info);
	graph = tracker_extract_info_get_graph (info);

	if (!f) {
		return FALSE;
	}

	if (ov_open (f, &vf, NULL, 0) < 0) {
		tracker_file_close (f, FALSE);
		return FALSE;
	}

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "ivi:Track");

	if ((comment = ov_comment (&vf, -1)) != NULL) {
		gchar *date;

		vd.title = ogg_get_comment (comment, "title");
		vd.artist = ogg_get_comment (comment, "artist");
		vd.album = ogg_get_comment (comment, "album");
		vd.album_artist = ogg_get_comment (comment, "albumartist");
		vd.track_count = ogg_get_comment (comment, "trackcount");
		vd.track_number = ogg_get_comment (comment, "tracknumber");
		vd.disc_number = ogg_get_comment (comment, "DiscNo");
		vd.performer = ogg_get_comment (comment, "Performer");
		vd.track_gain = ogg_get_comment (comment, "TrackGain");
		vd.track_peak_gain = ogg_get_comment (comment, "TrackPeakGain");
		vd.album_gain = ogg_get_comment (comment, "AlbumGain");
		vd.album_peak_gain = ogg_get_comment (comment, "AlbumPeakGain");

		date = ogg_get_comment (comment, "date");
		vd.date = tracker_date_guess (date);
		g_free (date);

		vd.comment = ogg_get_comment (comment, "comment");
		vd.genre = ogg_get_comment (comment, "genre");
		vd.codec = ogg_get_comment (comment, "Codec");
		vd.codec_version = ogg_get_comment (comment, "CodecVersion");
		vd.sample_rate = ogg_get_comment (comment, "SampleRate");
		vd.channels = ogg_get_comment (comment, "Channels");
		vd.mb_album_id = ogg_get_comment (comment, "MBAlbumID");
		vd.mb_artist_id = ogg_get_comment (comment, "MBArtistID");
		vd.mb_album_artist_id = ogg_get_comment (comment, "MBAlbumArtistID");
		vd.mb_track_id = ogg_get_comment (comment, "MBTrackID");
		vd.lyrics = ogg_get_comment (comment, "Lyrics");
		vd.copyright = ogg_get_comment (comment, "Copyright");
		vd.license = ogg_get_comment (comment, "License");
		vd.organization = ogg_get_comment (comment, "Organization");
		vd.location = ogg_get_comment (comment, "Location");
		vd.publisher = ogg_get_comment (comment, "Publisher");

		vorbis_comment_clear (comment);
	}

	md.creator = tracker_coalesce_strip (3, vd.artist, vd.album_artist, vd.performer);

	if (md.creator) {
		/* NOTE: This must be created before vd.album is evaluated */
		md.creator_uri = tracker_sparql_escape_uri_printf ("urn:artist:%s", md.creator);

		tracker_sparql_builder_insert_open (preupdate, NULL);
		if (graph) {
			tracker_sparql_builder_graph_open (preupdate, graph);
		}

		tracker_sparql_builder_subject_iri (preupdate, md.creator_uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "ivi:Artist");
		tracker_sparql_builder_predicate (preupdate, "ivi:artistname");
		tracker_sparql_builder_object_unvalidated (preupdate, md.creator);

		if (graph) {
			tracker_sparql_builder_graph_close (preupdate);
		}
		tracker_sparql_builder_insert_close (preupdate);

		tracker_sparql_builder_predicate (metadata, "ivi:trackartist");
		tracker_sparql_builder_object_iri (metadata, md.creator_uri);
	}

	if (vd.album) {
		gchar *uri = tracker_sparql_escape_uri_printf ("urn:album:%s", vd.album);
		gchar *album_disc_uri;

		tracker_sparql_builder_insert_open (preupdate, NULL);
		if (graph) {
			tracker_sparql_builder_graph_open (preupdate, graph);
		}

		tracker_sparql_builder_subject_iri (preupdate, uri);
		tracker_sparql_builder_predicate (preupdate, "a");
		tracker_sparql_builder_object (preupdate, "ivi:Album");
		tracker_sparql_builder_predicate (preupdate, "ivi:albumname");
		tracker_sparql_builder_object_unvalidated (preupdate, vd.album);

		if (md.creator_uri) {
			tracker_sparql_builder_predicate (preupdate, "ivi:albumalbumartist");
			tracker_sparql_builder_object_iri (preupdate, md.creator_uri);
		}

		if (graph) {
			tracker_sparql_builder_graph_close (preupdate);
		}
		tracker_sparql_builder_insert_close (preupdate);
	}

	g_free (vd.track_count);
	g_free (vd.album_peak_gain);
	g_free (vd.album_gain);
	g_free (vd.disc_number);

	if (vd.title) {
		tracker_sparql_builder_predicate (metadata, "ivi:trackname");
		tracker_sparql_builder_object_unvalidated (metadata, vd.title);
		g_free (vd.title);
	}

	if (vd.track_number) {
		tracker_sparql_builder_predicate (metadata, "ivi:tracktracknumber");
		tracker_sparql_builder_object_unvalidated (metadata, vd.track_number);
		g_free (vd.track_number);
	}

	tracker_sparql_builder_predicate (metadata, "ivi:filecreated");
	if (vd.date) {
		tracker_sparql_builder_object_unvalidated (metadata, vd.date);
		g_free (vd.date);
	} else {
		gchar *date;
		guint64 mtime;

		mtime = tracker_file_get_mtime_uri (file_uri);
		date = tracker_date_to_string ((time_t) mtime);
		tracker_sparql_builder_object_unvalidated (metadata, date);
		g_free(date);
	}

	if (vd.genre) {
		tracker_sparql_builder_predicate (metadata, "ivi:trackgenre");
		tracker_sparql_builder_object_unvalidated (metadata, vd.genre);
		g_free (vd.genre);
	}

	g_free (vd.artist);
	g_free (vd.album_artist);
	g_free (vd.performer);

	g_free (md.creator_uri);

#ifdef HAVE_POSIX_FADVISE
	posix_fadvise (fileno (f), 0, 0, POSIX_FADV_DONTNEED);
#endif /* HAVE_POSIX_FADVISE */

	/* NOTE: This calls fclose on the file */
	ov_clear (&vf);

	return TRUE;
}
