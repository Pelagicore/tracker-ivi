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

#define RFC1123_DATE_FORMAT "%d %B %Y %H:%M:%S %z"
const guchar png_signature[8] = {0x89, 0x50, 0x4e, 0x47,
                                        0xd,  0xa,  0x1a, 0xa};

typedef struct png_chunk {
	guint   length;   /* length of data field */
	gchar   type[4];  /* character identifier of chunk type */
	goffset offset;   /* Offset in file for the data field */
	gchar   crc[4];   /* CRC of chunk */
} PNGCHunk;

typedef struct file_props {
	      TrackerSparqlBuilder *pre_update;
	      TrackerSparqlBuilder *main_update;
	      TrackerSparqlBuilder *post_update;
	      gchar                *filename;
	      GFileInputStream     *file_is;
	      gchar                *uri;
	const gchar                *graph;
} PNGFileProps;

/* Used to contain the IHDR when it has been fread'ed */
#pragma pack(push, 1)
typedef struct png_header {
	guchar width[4];
	guchar height[4];
	guchar bit_depth;
	guchar color_type;
	guchar compression_method;
	guchar filter_method;
	guchar interlace_method;
} PNGHeader;
#pragma pack(pop)

typedef struct {
	gchar *author;
	gchar *creator;
	gchar *description;
	gchar *comment;
	gchar *copyright;
	gchar *title;
	gchar *disclaimer;
	gchar *make;
	gchar *model;
	gchar *license;
	gchar *creation_time;
	guint  width;
	guint  height;
} PNGProps;

static guint    four_bytes_to_uint  (guchar            bytes[static 4]);
static void     process_end         (void             *data,
                                     void            (*end_cb) (void *));
static void     process_text        (PNGCHunk         *chunk,
                                     GFileInputStream *fis,
                                     void             *data,
                                     void            (*text_cb) (gchar *,
                                                                 gchar *,
                                                                 gchar *,
                                                                 void  *));
static void     process_iTXt        (PNGCHunk         *chunk,
                                     GFileInputStream *fis,
                                     void             *data,
                                     void            (*text_cb) (gchar *,
                                                                 gchar *,
                                                                 gchar *,
                                                                 void  *));
static void     process_header      (PNGCHunk         *chunk,
                                     GFileInputStream *fp,
                                     void             *data,
                                     void            (*header_cb)
                                                          (PNGHeader *,
                                                           void      *));
static gboolean is_png_signature    (gchar            *bytes);
static gboolean get_next_chunk      (PNGCHunk         *chunk,
                                     GFileInputStream *png);
static gboolean read_file           (PNGFileProps     *fileprops,
                                     gboolean          skipahead,
                                     void             *props,
                                     void            (*text_cb) (gchar *,
                                                                 gchar *,
                                                                 gchar *,
                                                                 void  *),
                                     void            (*header_cb)
                                                     ( PNGHeader *, void *),
                                     void            (*end_cb) (void *));
static void     process_text_st_cb  (gchar            *field_name,
                                     gchar            *field_contents,
                                     gchar            *contents_encoding,
                                     void             *data);
static void     process_end_st_cb   (void             *data);
static void     process_header_st_cb(PNGHeader        *header,
                                     void             *data);
static void     insert_metadata     (PNGFileProps     *file_props,
                                     PNGProps         *metadata_props);

static guint
four_bytes_to_uint(guchar bytes[static 4])
{
	return (bytes[0] << 24) |
	       (bytes[1] << 16) |
	       (bytes[2] << 8)  |
	       (bytes[3] & 0xFF);
}

/* Given 8 bytes, decides if they are a PNG signature */
static gboolean
is_png_signature(gchar *bytes)
{
	return memcmp(bytes, png_signature, 8) == 0;
}

/* Read from a GInputStream until a specific character is encountered */
static gboolean
input_stream_read_until(GInputStream  *is,
                        gchar          until,
                        gsize         *bytes_read,
                        gchar         *buffer,
                        gsize          buffer_size,
                        GError       **error) {
	gchar c = TRUE;
	while (c != until && (*bytes_read) < buffer_size) {
		g_input_stream_read(is, &c, 1, NULL, error);
		if (*error)
			return FALSE;
		buffer[(*bytes_read)++] = c;
	}
	return TRUE;
}

/* Called when the IEND chunk is encountered */
static void
process_end(void *data,
            void (*end_cb) (void *))
{
	end_cb(data);
}

/* Called when iEXt chunks are encountered */
static void
process_iTXt(PNGCHunk         *chunk,
             GFileInputStream *fis,
             void             *data,
             void            (*text_cb) (gchar *, gchar *, gchar *, void *))
{
	/*
	 * language and trans_keyword and contents are never this long in
	 * practice, but the lengths are unknown, and they can not be longer
	 * than chunk->length
	 */
	gchar  *contents      = g_malloc(sizeof(gchar) * chunk->length);
	gchar  *language      = g_malloc(sizeof(gchar) * chunk->length);
	gchar  *trans_keyword = g_malloc(sizeof(gchar) * chunk->length);

	gchar   descriptor[80];
	gsize   desc_len  = 0, content_len = 0, lang_len = 0, trans_kw_len = 0;
	guchar  comp_flag = 0, comp_method = 0;
	GError *error;

	g_seekable_seek(G_SEEKABLE(fis),
	                chunk->offset,
	                G_SEEK_SET,
	                NULL,
	                &error);
	if (error)
		g_warning("Failed to seek in text chunk");

	input_stream_read_until(G_INPUT_STREAM(fis),
	                        '\0',
	                        &desc_len,
	                        descriptor,
	                        sizeof(descriptor),
	                        &error);
	if (error)
		g_warning("Failed to read description field: %s",
		          error->message);

	g_input_stream_read(G_INPUT_STREAM(fis),
	                    &comp_flag,
                            sizeof(comp_flag),
                            NULL,
                            &error);
	if (error)
		g_warning("Failed to read compression flag in iTXt");

	g_input_stream_read(G_INPUT_STREAM(fis),
	                    &comp_method,
                            sizeof(comp_method),
                            NULL,
                            &error);
	if (error)
		g_warning("Failed to read compression method in iTXt");

	input_stream_read_until(G_INPUT_STREAM(fis),
	                        '\0',
	                        &lang_len,
	                        language,
	                        sizeof(language),
	                        &error);
	if (error)
		g_warning("Failed to read language field: %s", error->message);

	input_stream_read_until(G_INPUT_STREAM(fis),
	                        '\0',
	                        &trans_kw_len,
	                        trans_keyword,
	                        sizeof(trans_keyword),
	                        &error);
	if (error)
		g_warning("Failed to read translated keyword field: %s",
		          error->message);

	/* Actual content is composed of the remaining bytes */
	content_len = chunk->length      -
	              desc_len           -
                      lang_len           -
                      trans_kw_len       -
                      sizeof(comp_flag)  -
                      sizeof(comp_method);

	g_input_stream_read(G_INPUT_STREAM(fis),
	                                  contents,
					  content_len,
					  NULL, &error);

	if (error)
		g_warning("Failed to read contents: %s", error->message);

	contents[content_len] = '\0';

	/* TODO: Do we want to verify the CRC? */
	g_seekable_seek(G_SEEKABLE(fis), 4, G_SEEK_CUR, NULL, &error);
	if (error)
		g_warning("Failed to skip CRC");

	/* Call user callback, but only on uncompressed data*/
	if (comp_flag)
		g_warning("Compression is not supported for iTXt chunks");
	else
		text_cb(descriptor, contents, "UTF-8", data);

	g_free(contents);
	g_free(language);
	g_free(trans_keyword);
	if (error)
		g_error_free(error);
}

/* Called when tEXt chunks are encountered */
static void
process_text(PNGCHunk         *chunk,
             GFileInputStream *fis,
             void             *data,
             void            (*text_cb) (gchar *, gchar *, gchar *, void *))
{
	gchar   *contents = g_malloc((sizeof(gchar) * chunk->length) + 1);
	gchar   descriptor[80];
	gsize   desc_len = 0, content_len = 0;
	GError *error;

	g_seekable_seek(G_SEEKABLE(fis),
	                chunk->offset,
	                G_SEEK_SET,
	                NULL,
	                &error);
	if (error)
		g_warning("Failed to seek in text chunk");

	input_stream_read_until(G_INPUT_STREAM(fis),
	                        '\0',
	                        &desc_len,
	                        descriptor,
	                        sizeof(descriptor),
	                        &error);

	if (error) {
		g_warning("Failed to read description field: %s",
		          error->message);
	}

	content_len = g_input_stream_read(G_INPUT_STREAM(fis),
	                                  contents, chunk->length - desc_len,
	                                  NULL, &error);
	if (error || content_len != chunk->length - desc_len)
		g_warning("Failed to read contents: %s",
		           error ? error->message : "No message");
	contents[chunk->length - desc_len] = '\0';

	/* TODO: Do we want to verify the CRC? */
	g_seekable_seek(G_SEEKABLE(fis), 4, G_SEEK_CUR, NULL, &error);
	if (error)
		g_warning("Failed to skip CRC");

	/* Call user callback */
	text_cb(descriptor, contents, "LATIN1", data);

	g_free(contents);
	if (error)
		g_error_free(error);
}

/* Called when the IHDR chunk is encountered */
static void
process_header(PNGCHunk         *chunk,
               GFileInputStream *fis,
               void             *data,
               void            (*header_cb) (PNGHeader *, void*))
{
	PNGHeader *header = g_malloc(sizeof(*header));
	GError    *error = NULL;

	g_seekable_seek(G_SEEKABLE(fis),
	               chunk->offset,
	               G_SEEK_SET,
	               NULL,
	               &error);
        if (error)
		g_warning("Failed to seek in header");

	g_input_stream_read(G_INPUT_STREAM(fis),
	                    header,
	                    sizeof(*header),
	                    NULL,
	                    &error);
	if (error)
		g_warning("Failed to read IHDR field");

	header_cb(header, data);

	/* Skip CRC */
	g_seekable_seek(G_SEEKABLE(fis), 4, G_SEEK_CUR, NULL, &error);
        if (error)
		g_warning("Failed to skip CRC in header");

	if (error)
		g_error_free(error);
	g_free(header);
}

/* Reads a chunk of data from the PNG into the chunk parameter */
static gboolean
get_next_chunk(PNGCHunk         *chunk,
               GFileInputStream *png)
{
	gchar         size[4], type[4], crc[4];
	gint16        ret;
	GError       *error  = NULL;
	GInputStream *is     = G_INPUT_STREAM(png);
	gboolean      retval = TRUE;

	ret = g_input_stream_read(is, size, sizeof(size), NULL, &error);
	if (error || ret != sizeof(size)){
		g_warning("Failed to read data size for chunk: %s",
		           error ? error->message : "No message");
		retval = FALSE;
		goto cleanup;
	}

	/* Set the length of the data field */
	chunk->length = four_bytes_to_uint(size);

	/* Set the type of the chunk */
	ret = g_input_stream_read(is, type, sizeof(type), NULL, &error);
	if (error || ret != sizeof(type)) {
		g_warning("Failed to read data type for chunk: %s",
		           error ? error->message : "No message");
		retval = FALSE;
		goto cleanup;
	}
	memcpy(chunk->type, type, sizeof(type));

	/* Set the offset of the chunk */
	chunk->offset = g_seekable_tell(G_SEEKABLE(png));
	g_seekable_seek(G_SEEKABLE(png),
	                chunk->length,
	                G_SEEK_CUR,
	                NULL,
	                &error);
	if (error) {
		g_warning("Failed to seek in chunk: %s",
		          error ? error->message : "No message");
		retval = FALSE;
		goto cleanup;
	}

	/* Set the CRC of the chunk */
	ret = g_input_stream_read(is, crc, sizeof(crc), NULL, &error);
	if (error || ret != sizeof(crc)) {
		g_warning("Failed to read crc of chunk: %s",
		          error ? error->message : "No message");
		retval = FALSE;
		goto cleanup;
	}

	memcpy(chunk->crc, crc, sizeof(crc));

cleanup:
	g_free(error);
	return retval;
}

/*
 * Process a PNG file, text_cb is called on each text chunk, header_cb on the
 * header, and end_cb when IEND is encountered
 */
static gboolean
read_file(PNGFileProps *fileprops,
          gboolean      skipahead,
          void         *props,
          void        (*text_cb)   (gchar *, gchar *, gchar *, void *),
          void        (*header_cb) (PNGHeader *, void *),
          void        (*end_cb)    (void *))
{
	gchar             sig[8];
	goffset           filesize;
	GError           *error = NULL;
	GFileInputStream *fis;
	GInputStream     *is;
	GFileInfo        *fileinfo = NULL;
	gboolean          retval   = TRUE;

	fis = fileprops->file_is;
	is  = G_INPUT_STREAM(fis);
	fileinfo = g_file_input_stream_query_info(
                           fis,
                           G_FILE_ATTRIBUTE_STANDARD_SIZE,
                           NULL,
                           &error);

	if (error) {
		g_warning("Unable to retrieve file size");
		retval = FALSE;
		goto cleanup;
	}

	filesize = g_file_info_get_size(fileinfo);

	if (g_input_stream_read(is, sig, sizeof(sig), NULL, &error) !=
	    sizeof(sig)) {
		g_warning("Failed to read PNG header!");
		retval = FALSE;
		goto cleanup;
	}

	if (!is_png_signature(sig)) {
		g_warning("This is not a PNG!");
		retval = FALSE;
		goto cleanup;
	}

	while (1) {
		PNGCHunk chunk;
		if (!get_next_chunk(&chunk, fis)) {
			g_warning("Failed to read chunk!");
			retval = FALSE;
			goto cleanup;
		}
		if (strncmp(chunk.type, "IDAT", 4) == 0) {
			if (skipahead) {
				long pos;
				int skips;
				goffset total;
				pos = g_seekable_tell(G_SEEKABLE(fis));
				/* Estimate the number of chunks in the PNG,
				 * assuming the n-1 first chunks are equal
				 * sized. Remove the n:th chunk, since it is
				 * likely smaller than the n first. Add 12
				 * bytes to each chunk to account for (chunk
				 * size + chunk identifier + CRC). Also remove
				 * the currently processed data (pos). */
				skips = (filesize - chunk.length - pos) /
				         chunk.length;
				total = (chunk.length + 12) * skips;
				g_seekable_seek(G_SEEKABLE(fis),
				                total,
				                G_SEEK_CUR,
				                NULL,
				                &error);
				skipahead = FALSE;
				if (error) {
					g_warning("Failed to skip "
					          "IDAT chunks!");
					retval = FALSE;
					goto cleanup;
				}
			}
		} else if (strncmp(chunk.type, "tEXt", 4) == 0) {
			process_text(&chunk,
			             fis,
			             (void *) props,
			             text_cb);
		} else if (strncmp(chunk.type, "iTXt", 4) == 0) {
			process_iTXt(&chunk,
			             fis,
			             (void *) props,
			             text_cb);
		} else if (strncmp(chunk.type, "IHDR", 4) == 0) {
			process_header(&chunk,
			               fis,
			               (void *) props,
			               header_cb);
		} else if (strncmp(chunk.type, "IEND", 4) == 0) {
			process_end((void  *) props,
			           end_cb);
			break;
		}
	}
cleanup:
	g_object_unref(fileinfo);
	if (error)
		g_error_free(error);
	return retval;
}

static void
process_text_st_cb(gchar *field_name,
                   gchar *field_contents,
                   gchar *codeset,
                   void  *data)
{
	PNGProps *props = (PNGProps *) data;
	gchar *utf8_field_contents = NULL;

	if (!codeset) {
		g_warning("No codeset specified in process_text_st_cb. "
	                  "Skipping field '%s'!",
		          field_name ? field_name : "");
		return;
	}

	if (field_contents) {
		gsize bytes_read = 0, bytes_written = 0;
		GError *error = NULL;
		utf8_field_contents = g_convert(field_contents,
		                                -1,
	                                        "UTF-8",
		                                codeset,
		                                &bytes_read,
		                                &bytes_written,
		                                &error);
		if (error) {
			g_warning("Failed to convert metadata field '%s' "
			          "from '%s' to UTF-8. Skipping field. "
				  "Error message: %s",
			          field_name ? field_name : "",
			          codeset,
				  error->message);
			return;
		}
	}

	if (g_strcmp0(field_name, "Comment") == 0)
		props->comment = utf8_field_contents;
	else if (g_strcmp0(field_name, "Author") == 0)
		props->creator = utf8_field_contents;
	else if (g_strcmp0(field_name, "Description") == 0)
		props->description = utf8_field_contents;
	else if (g_strcmp0(field_name, "Copyright") == 0)
		props->copyright = utf8_field_contents;
	else if (g_strcmp0(field_name, "Creation Time") == 0)
		props->creation_time =
		  tracker_date_format_to_iso8601(utf8_field_contents,
		                                 RFC1123_DATE_FORMAT);
	else if (g_strcmp0(field_name, "Title") == 0)
		props->title = utf8_field_contents;
	else if (g_strcmp0(field_name, "Disclaimer") == 0)
		props->disclaimer = utf8_field_contents;
	else if (g_strcmp0(field_name, "Model") == 0)
		props->model = utf8_field_contents;
	else if (g_strcmp0(field_name, "Make") == 0)
		props->make = utf8_field_contents;
	else if (g_strcmp0(field_name, "License") == 0)
		props->license = utf8_field_contents;
}
static void
process_end_st_cb(void *data) {
  /* Nothing needs to be done here.. */
}

static void
process_header_st_cb(PNGHeader *header,
                     void      *data)
{
	PNGProps *props = (PNGProps *) data;
	props->width  = four_bytes_to_uint(header->width);
	props->height = four_bytes_to_uint(header->height);
}

static void
insert_metadata(PNGFileProps *file_props,
                PNGProps     *metadata_props)
{
	const gchar                *graph;
	      TrackerSparqlBuilder *metadata;
	      TrackerSparqlBuilder *preupdate;
	      gchar                *dlna_profile;

	dlna_profile = NULL;
	graph        = file_props->graph;
	preupdate    = file_props->pre_update;
	metadata     = file_props->main_update;

	if (metadata_props->comment) {
		tracker_sparql_builder_predicate(metadata,
		                                 "ivi:imagecomment");
		tracker_sparql_builder_object_unvalidated(metadata,
		                            metadata_props->comment);
	}

	if (metadata_props->creator) {
		gchar *uri;
		uri = tracker_sparql_escape_uri_printf("urn:artist:%s",
		                            metadata_props->creator);

		tracker_sparql_builder_insert_open(preupdate, NULL);
		if (graph)
			tracker_sparql_builder_graph_open(preupdate, graph);

		tracker_sparql_builder_subject_iri(preupdate, uri);
		tracker_sparql_builder_predicate(preupdate, "a");
		tracker_sparql_builder_object(preupdate, "ivi:Artist");
		tracker_sparql_builder_predicate(preupdate, "ivi:artistname");
		tracker_sparql_builder_object_unvalidated(preupdate,
		                            metadata_props->creator);

		if (graph)
			tracker_sparql_builder_graph_close(preupdate);

		tracker_sparql_builder_insert_close(preupdate);

		tracker_sparql_builder_predicate(metadata, "ivi:imagecreator");
		tracker_sparql_builder_object_iri(metadata, uri);
		g_free(uri);
	}

	if (metadata_props->description) {
		tracker_sparql_builder_predicate(metadata,
		                                 "ivi:imagedescription");
		tracker_sparql_builder_object_unvalidated(metadata,
		                            metadata_props->description);
	}

	if (metadata_props->copyright) {
		tracker_sparql_builder_predicate(metadata,
		                                 "ivi:imagecopyright");
		tracker_sparql_builder_object_unvalidated(metadata,
		                            metadata_props->copyright);
	}


	if (metadata_props->make) {
		tracker_sparql_builder_predicate(metadata,
						  "ivi:cameramanufacturer");
		tracker_sparql_builder_object_unvalidated(metadata,
				    metadata_props->make);
	}
	if (metadata_props->model) {
		tracker_sparql_builder_predicate(metadata,
						  "ivi:cameramodel");
		tracker_sparql_builder_object_unvalidated(metadata,
				    metadata_props->model);
	}

	tracker_guarantee_date_from_file_mtime(metadata,
	                                        "ivi:imagedate",
	                                        metadata_props->creation_time,
	                                        file_props->uri);

	tracker_guarantee_title_from_file(metadata,
	                                   "ivi:imagetitle",
	                                   metadata_props->title,
	                                   file_props->uri,
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
	      PNGFileProps          props;
	      GError               *error = NULL;
	      PNGProps              metadata_props = { 0 };
	      gboolean              retval = TRUE;

	file         = tracker_extract_info_get_file(info);
	filename     = g_file_get_path(file);
	uri          = g_file_get_uri(file);
	where        = g_string_new("");

	pre_builder  = tracker_extract_info_get_preupdate_builder(info);
	builder      = tracker_extract_info_get_metadata_builder(info);
	post_builder = tracker_extract_info_get_postupdate_builder(info);
	graph        = tracker_extract_info_get_graph(info);

	props.main_update = builder;
	props.pre_update  = pre_builder;
	props.post_update = post_builder;
	props.filename    = filename;
	props.graph       = graph;
	props.uri         = uri;
	props.file_is     = g_file_read(file, NULL, &error);

	if (error) {
		g_warning("Unable to read file!");
		retval = FALSE;
		goto cleanup;
	}

	/* Gather metadata from header and text fields */
	if (!read_file(&props,
		       TRUE, /* TRUE = Skip data sections when reading file */
		       (void *) &metadata_props,
		       process_text_st_cb,
		       process_header_st_cb,
		       process_end_st_cb)) {
		g_warning("Failed to read file");
		retval = FALSE;
		goto cleanup;
	}

	tracker_sparql_builder_predicate(builder, "a");
	tracker_sparql_builder_object(builder, "ivi:Image");

	/* Width and height are guaranteed to be present in the header.. */
	tracker_sparql_builder_predicate(builder, "ivi:imagewidth");
	tracker_sparql_builder_object_int64(builder, metadata_props.width);

	tracker_sparql_builder_predicate(builder, "ivi:imageheight");
	tracker_sparql_builder_object_int64(builder, metadata_props.height);

	/* Insert properties found in text fields */
	insert_metadata(&props, &metadata_props);

	tracker_extract_info_set_where_clause(info, where->str);

cleanup:
	g_string_free(where, TRUE);
	g_free(filename);
	g_free(uri);
	g_object_unref(props.file_is);
	g_free(metadata_props.author);
	g_free(metadata_props.creator);
	g_free(metadata_props.description);
	g_free(metadata_props.comment);
	g_free(metadata_props.copyright);
	g_free(metadata_props.title);
	g_free(metadata_props.disclaimer);
	g_free(metadata_props.make);
	g_free(metadata_props.model);
	g_free(metadata_props.license);
	if (error)
		g_error_free(error);
	return retval;
}
