include $(top_srcdir)/Makefile.decl

module_flags = -module -avoid-version -no-undefined
modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/extract-modules
rulesdir = $(datadir)/tracker/extract-rules

AM_CPPFLAGS = \
	$(BUILD_CFLAGS) \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/src \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DTRACKER_EXTRACTORS_DIR=\"$(modulesdir)\" \
	$(TRACKER_EXTRACT_CFLAGS)

# NOTE: The rules are here and not in another sub-directory because we
# only install rules for extractors we build. If we decouple this, it
# duplicates work and requires more maintenance to keep modules up to
# date.
rules_in_files = \
	07-png-faster-ivi.rule.in \
	08-png-ivi.rule.in \
	09-gif-ivi.rule.in \
	09-jpeg-ivi.rule.in \
	09-mp3-ivi.rule.in \
	09-png-faster.rule.in \
	09-vorbis-ivi.rule.in \
	10-abw.rule.in \
	10-dvi.rule.in \
	10-epub.rule.in \
	10-flac.rule.in \
	10-gif.rule.in \
	10-html.rule.in \
	10-ico.rule.in \
	10-jpeg.rule.in \
	10-mp3.rule.in \
	10-msoffice.rule.in \
	10-oasis.rule.in \
	10-pdf.rule.in \
	10-png.rule.in \
	10-ps.rule.in \
	10-svg.rule.in \
	10-tiff.rule.in \
	10-vorbis.rule.in \
	10-xmp.rule.in \
	10-xps.rule.in \
	11-iso.rule.in \
	11-msoffice-xml.rule.in \
	14-libav-ivi.rule.in \
	15-gstreamer-guess.rule.in \
	15-playlist.rule.in \
	90-gstreamer-generic.rule.in \
	90-text-generic.rule.in \
	91-gstreamer-generic-dlna.rule.in \
	92-xine-generic.rule.in \
	93-mplayer-generic.rule.in \
	93-totem-generic.rule.in

%.rule: %.rule.in
	@sed -e "s|@modulesdir[@]|${modulesdir}|" $< > $@

# NOTE:
# We don't always link with libtracker-common, we only link
# against it if we directly use functions in the .so
modules_LTLIBRARIES = # Empty
rules_DATA = # Empty

if HAVE_LIBVORBIS
modules_LTLIBRARIES += libextract-vorbis.la
rules_DATA += 10-vorbis.rule
endif

if HAVE_LIBFLAC
modules_LTLIBRARIES += libextract-flac.la
rules_DATA += 10-flac.rule
endif

if HAVE_EXEMPI
modules_LTLIBRARIES += libextract-xmp.la
rules_DATA += 10-xmp.rule
endif

if HAVE_LIBGIF
modules_LTLIBRARIES += libextract-gif.la
rules_DATA += 10-gif.rule
endif

if HAVE_LIBJPEG
modules_LTLIBRARIES += libextract-jpeg.la
rules_DATA += 10-jpeg.rule
endif

if HAVE_LIBTIFF
modules_LTLIBRARIES += libextract-tiff.la
rules_DATA += 10-tiff.rule
endif

if HAVE_LIBXML2
modules_LTLIBRARIES += libextract-html.la
rules_DATA += 10-html.rule
endif

if HAVE_LIBGSF
modules_LTLIBRARIES += \
	libextract-epub.la \
	libextract-msoffice.la \
	libextract-msoffice-xml.la \
	libextract-oasis.la
rules_DATA += 10-epub.rule 10-oasis.rule 10-msoffice.rule 11-msoffice-xml.rule
endif

if HAVE_LIBGXPS
modules_LTLIBRARIES += libextract-xps.la
rules_DATA += 10-xps.rule
endif

if HAVE_POPPLER
modules_LTLIBRARIES += libextract-pdf.la
rules_DATA += 10-pdf.rule
endif

if HAVE_GSTREAMER
modules_LTLIBRARIES += libextract-gstreamer.la
rules_DATA += 10-svg.rule 15-gstreamer-guess.rule 90-gstreamer-generic.rule
endif

# Gstreamer with GUPnP-DLNA backend?
if HAVE_GUPNP_DLNA
rules_DATA += 91-gstreamer-generic-dlna.rule
endif

if HAVE_LIBXINE
modules_LTLIBRARIES += libextract-xine.la
rules_DATA += 92-xine-generic.rule
endif

if USING_EXTERNAL_GENERIC_MEDIA_PLAYER
modules_LTLIBRARIES +=	libextract-mplayer.la \
			libextract-totem.la
rules_DATA += 93-mplayer-generic.rule 93-totem-generic.rule
endif

if HAVE_TOTEM_PL_PARSER
modules_LTLIBRARIES += libextract-playlist.la
rules_DATA += 15-playlist.rule
endif

if HAVE_LIBOSINFO
modules_LTLIBRARIES += libextract-iso.la
rules_DATA += 11-iso.rule
endif

if HAVE_LIBPNG
modules_LTLIBRARIES += libextract-png.la
rules_DATA += 10-png.rule
endif

if HAVE_ABIWORD
modules_LTLIBRARIES += libextract-abw.la
rules_DATA += 10-abw.rule
endif

if HAVE_DVI
modules_LTLIBRARIES += libextract-dvi.la
rules_DATA += 10-dvi.rule
endif

if HAVE_MP3
modules_LTLIBRARIES += libextract-mp3.la
rules_DATA += 10-mp3.rule
endif

if HAVE_PS
modules_LTLIBRARIES += libextract-ps.la
rules_DATA += 10-ps.rule
endif

if HAVE_TEXT
modules_LTLIBRARIES += libextract-text.la
rules_DATA += 90-text-generic.rule
endif

if HAVE_ICON
modules_LTLIBRARIES += libextract-icon.la
rules_DATA += 10-ico.rule
endif

if HAVE_PNG_FASTER
modules_LTLIBRARIES += libextract-png-faster.la
rules_DATA += 09-png-faster.rule
endif

if HAVE_MP3_IVI
modules_LTLIBRARIES += libextract-mp3-ivi.la
rules_DATA += 09-mp3-ivi.rule
endif

if HAVE_PNG_IVI
modules_LTLIBRARIES += libextract-png-ivi.la
rules_DATA += 08-png-ivi.rule
endif

if HAVE_LIBJPEG_IVI
modules_LTLIBRARIES += libextract-jpeg-ivi.la
rules_DATA += 09-jpeg-ivi.rule
endif

if HAVE_LIBGIF_IVI
modules_LTLIBRARIES += libextract-gif-ivi.la
rules_DATA += 09-gif-ivi.rule
endif

if HAVE_PNG_FASTER_IVI
modules_LTLIBRARIES += libextract-png-faster-ivi.la
rules_DATA += 07-png-faster-ivi.rule
endif

if HAVE_LIBAV_IVI
modules_LTLIBRARIES += libextract-libav-ivi.la
rules_DATA += 14-libav-ivi.rule
endif

if HAVE_LIBVORBIS_IVI
modules_LTLIBRARIES += libextract-vorbis-ivi.la
rules_DATA += 09-vorbis-ivi.rule
endif

# ABW
libextract_abw_la_SOURCES = tracker-extract-abw.c
libextract_abw_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_abw_la_LDFLAGS = $(module_flags)
libextract_abw_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# MP3
libextract_mp3_la_SOURCES = tracker-extract-mp3.c
libextract_mp3_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_mp3_la_LDFLAGS = $(module_flags)
libextract_mp3_la_LIBADD =  \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# Vorbis (OGG)
libextract_vorbis_la_SOURCES = tracker-extract-vorbis.c $(escape_sources)
libextract_vorbis_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_vorbis_la_LDFLAGS = $(module_flags)
libextract_vorbis_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBVORBIS_LIBS)

# Flac
libextract_flac_la_SOURCES = tracker-extract-flac.c $(escape_sources)
libextract_flac_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_flac_la_LDFLAGS = $(module_flags)
libextract_flac_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBFLAC_LIBS)

# MPlayer
libextract_mplayer_la_SOURCES = tracker-extract-mplayer.c
libextract_mplayer_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_mplayer_la_LDFLAGS = $(module_flags)
libextract_mplayer_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# Oasis
libextract_oasis_la_SOURCES = tracker-extract-oasis.c
libextract_oasis_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBGSF_CFLAGS)
libextract_oasis_la_LDFLAGS = $(module_flags)
libextract_oasis_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBGSF_LIBS)

# EPub
libextract_epub_la_SOURCES = tracker-extract-epub.c
libextract_epub_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBGSF_CFLAGS)
libextract_epub_la_LDFLAGS = $(module_flags)
libextract_epub_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBGSF_LIBS)

# PNG
libextract_png_la_SOURCES = tracker-extract-png.c
libextract_png_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBPNG_CFLAGS)
libextract_png_la_LDFLAGS = $(module_flags)
libextract_png_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBPNG_LIBS)

# PNG Faster
libextract_png_faster_la_SOURCES = tracker-extract-png-faster.c
libextract_png_faster_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBPNG_CFLAGS)
libextract_png_faster_la_LDFLAGS = $(module_flags)
libextract_png_faster_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# PS
libextract_ps_la_SOURCES = tracker-extract-ps.c
libextract_ps_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_ps_la_LDFLAGS = $(module_flags)
libextract_ps_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# Totem
libextract_totem_la_SOURCES = tracker-extract-totem.c
libextract_totem_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_totem_la_LDFLAGS = $(module_flags)
libextract_totem_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# XMP
libextract_xmp_la_SOURCES = tracker-extract-xmp.c
libextract_xmp_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_xmp_la_LDFLAGS = $(module_flags)
libextract_xmp_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# HTML
libextract_html_la_SOURCES = tracker-extract-html.c
libextract_html_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBXML2_CFLAGS)
libextract_html_la_LDFLAGS = $(module_flags)
libextract_html_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBXML2_LIBS)

# MS Office
libextract_msoffice_la_SOURCES = tracker-extract-msoffice.c
libextract_msoffice_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBGSF_CFLAGS)
libextract_msoffice_la_LDFLAGS = $(module_flags)
libextract_msoffice_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBGSF_LIBS)

# MS Office XML
libextract_msoffice_xml_la_SOURCES = tracker-extract-msoffice-xml.c
libextract_msoffice_xml_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBGSF_CFLAGS)
libextract_msoffice_xml_la_LDFLAGS = $(module_flags)
libextract_msoffice_xml_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# XPS
libextract_xps_la_SOURCES = tracker-extract-xps.c
libextract_xps_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBGXPS_CFLAGS)
libextract_xps_la_LDFLAGS = $(module_flags)
libextract_xps_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBGXPS_LIBS)

# PDF
libextract_pdf_la_SOURCES = tracker-extract-pdf.c
libextract_pdf_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(POPPLER_CFLAGS)
libextract_pdf_la_LDFLAGS = $(module_flags)
libextract_pdf_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(POPPLER_LIBS)

# DVI
libextract_dvi_la_SOURCES = tracker-extract-dvi.c
libextract_dvi_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_dvi_la_LDFLAGS = $(module_flags)
libextract_dvi_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# GStreamer
libextract_gstreamer_la_SOURCES = \
	tracker-extract-gstreamer.c \
	tracker-cue-sheet.h \
	tracker-cue-sheet.c
libextract_gstreamer_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(LIBCUE_CFLAGS)
libextract_gstreamer_la_LDFLAGS = $(module_flags)
libextract_gstreamer_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(GSTREAMER_LIBS) \
	$(LIBCUE_LIBS)

# GStreamer with GUPnP-DLNA backend?
if HAVE_GUPNP_DLNA
libextract_gstreamer_la_CFLAGS += $(GUPNP_DLNA_CFLAGS)
libextract_gstreamer_la_LIBADD += $(GUPNP_DLNA_LIBS)
endif

# GStreamer with Discoverer backend?
if HAVE_GSTREAMER_PBUTILS
libextract_gstreamer_la_CFLAGS += $(GSTREAMER_PBUTILS_CFLAGS)
libextract_gstreamer_la_LIBADD += $(GSTREAMER_PBUTILS_LIBS)
endif

# Xine
libextract_xine_la_SOURCES = tracker-extract-libxine.c
libextract_xine_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(XINE_CFLAGS)
libextract_xine_la_LDFLAGS = $(module_flags)
libextract_xine_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(XINE_LIBS)

# GIF
libextract_gif_la_SOURCES = tracker-extract-gif.c
libextract_gif_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBGIF_CFLAGS)
libextract_gif_la_LDFLAGS = $(module_flags)
libextract_gif_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBGIF_LIBS)

# JPEG
libextract_jpeg_la_SOURCES = tracker-extract-jpeg.c
libextract_jpeg_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBJPEG_CFLAGS)
libextract_jpeg_la_LDFLAGS = $(module_flags)
libextract_jpeg_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBJPEG_LIBS)

# TIFF
libextract_tiff_la_SOURCES = tracker-extract-tiff.c $(xmp_sources) $(iptc_sources)
libextract_tiff_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBTIFF_CFLAGS)
libextract_tiff_la_LDFLAGS = $(module_flags)
libextract_tiff_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBTIFF_LIBS)

# Playlists using totem-pl-parser
libextract_playlist_la_SOURCES = tracker-extract-playlist.c
libextract_playlist_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(TOTEM_PL_PARSER_CFLAGS)
libextract_playlist_la_LDFLAGS = $(module_flags)
libextract_playlist_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(TOTEM_PL_PARSER_LIBS)

# TEXT
libextract_text_la_SOURCES = tracker-extract-text.c
libextract_text_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_text_la_LDFLAGS = $(module_flags)
libextract_text_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# ICON
libextract_icon_la_SOURCES = tracker-extract-icon.c
libextract_icon_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_icon_la_LDFLAGS = $(module_flags)
libextract_icon_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# ISO
libextract_iso_la_SOURCES = tracker-extract-iso.c
libextract_iso_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBOSINFO_CFLAGS)

libextract_iso_la_LDFLAGS = $(module_flags)
libextract_iso_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBOSINFO_LIBS)

# MP3-IVI
libextract_mp3_ivi_la_SOURCES = tracker-extract-mp3-ivi.c
libextract_mp3_ivi_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_mp3_ivi_la_LDFLAGS = $(module_flags)
libextract_mp3_ivi_la_LIBADD =  \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# PNG-IVI
libextract_png_ivi_la_SOURCES = tracker-extract-png-ivi.c
libextract_png_ivi_la_CFLAGS = $(TRACKER_EXTRACT_MODULES_CFLAGS)
libextract_png_ivi_la_LDFLAGS = $(module_flags)
libextract_png_ivi_la_LIBADD =  \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# JPEG-IVI
libextract_jpeg_ivi_la_SOURCES = tracker-extract-jpeg-ivi.c
libextract_jpeg_ivi_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBJPEG_IVI_CFLAGS)
libextract_jpeg_ivi_la_LDFLAGS = $(module_flags)
libextract_jpeg_ivi_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBJPEG_IVI_LIBS)

# PNG Faster-ivi
libextract_png_faster_ivi_la_SOURCES = tracker-extract-png-faster-ivi.c
libextract_png_faster_ivi_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBPNG_CFLAGS)
libextract_png_faster_ivi_la_LDFLAGS = $(module_flags)
libextract_png_faster_ivi_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS)

# GIF-ivi
libextract_gif_ivi_la_SOURCES = tracker-extract-gif-ivi.c
libextract_gif_ivi_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBGIF_IVI_CFLAGS)
libextract_gif_ivi_la_LDFLAGS = $(module_flags)
libextract_gif_ivi_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBGIF_IVI_LIBS)

# libav-ivi
libextract_libav_ivi_la_SOURCES = tracker-extract-libav-ivi.c
libextract_libav_ivi_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBAV_CFLAGS)
libextract_libav_ivi_la_LDFLAGS = $(module_flags)
libextract_libav_ivi_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBAV_LIBS)

# vorbis-ivi
libextract_vorbis_ivi_la_SOURCES = tracker-extract-vorbis-ivi.c
libextract_vorbis_ivi_la_CFLAGS = \
	$(TRACKER_EXTRACT_MODULES_CFLAGS) \
	$(LIBAV_CFLAGS)
libextract_vorbis_ivi_la_LDFLAGS = $(module_flags)
libextract_vorbis_ivi_la_LIBADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_MODULES_LIBS) \
	$(LIBVORBIS_IVI_LIBS)

#
# Binaries
#
libexec_PROGRAMS = tracker-extract

tracker_extract_SOURCES = \
	$(marshal_sources) \
	tracker-config.c \
	tracker-config.h \
	tracker-controller.c \
	tracker-controller.h \
	tracker-extract.c \
	tracker-extract.h \
	tracker-media-art.c \
	tracker-media-art.h \
	tracker-read.c \
	tracker-read.h \
	tracker-main.c \
	tracker-main.h \
	tracker-media-art-generic.h

tracker_extract_LDADD = \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_EXTRACT_LIBS)

if HAVE_LIBGSF
tracker_extract_SOURCES += tracker-gsf.c tracker-gsf.h
tracker_extract_CFLAGS = $(LIBGSF_CFLAGS)
tracker_extract_LDADD += $(LIBGSF_LIBS)
endif

if HAVE_LIBSTREAMANALYZER
tracker_extract_SOURCES += tracker-topanalyzer.cpp tracker-topanalyzer.h
endif

if HAVE_GDKPIXBUF
tracker_extract_SOURCES += tracker-media-art-pixbuf.c
else
if HAVE_QT
tracker_extract_SOURCES += tracker-media-art-qt.cpp
else
tracker_extract_SOURCES += tracker-media-art-dummy.c
endif
endif

marshal_sources = \
        tracker-marshal.h \
        tracker-marshal.c

tracker-marshal.h: tracker-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	$(AM_V_GEN)echo "#include \"tracker-marshal.h\"" > $@ && \
		   $(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body >> $@

BUILT_SOURCES = $(marshal_sources)

CLEANFILES = $(BUILT_SOURCES) $(rules_DATA)

EXTRA_DIST = \
	tracker-marshal.list \
	$(rules_in_files)
