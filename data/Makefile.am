include $(top_srcdir)/Makefile.decl

SUBDIRS = dbus languages icons ontologies miners gschemas

# First we fix the VERSION and libexecdir in the desktop files
tracker-store.desktop.in: tracker-store.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|"      \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-miner-fs.desktop.in: tracker-miner-fs.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|"      \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-miner-rss.desktop.in: tracker-miner-rss.desktop.in.in
	@$(SED) -e "s|@libexecdir[@]|${libexecdir}|"   \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

desktop_in_in_files =                                  \
	tracker-store.desktop.in.in                    \
	tracker-miner-fs.desktop.in.in                 \
	tracker-miner-rss.desktop.in.in

# Second take those new .in files and translate them
desktop_in_files =                                     \
	tracker-store.desktop.in                       \
	tracker-miner-fs.desktop.in                    \
	tracker-miner-rss.desktop.in

# Third we choose what we want installed based on config
autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA =                                       \
	tracker-store.desktop

if HAVE_TRACKER_MINER_FS
autostart_DATA += tracker-miner-fs.desktop
endif

if HAVE_TRACKER_MINER_RSS
autostart_DATA += tracker-miner-rss.desktop
endif

@INTLTOOL_DESKTOP_RULE@

# Set up pkg-config .pc files for exported libraries
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =                                       \
	tracker-sparql-$(TRACKER_API_VERSION).pc       \
	tracker-extract-$(TRACKER_API_VERSION).pc      \
	tracker-miner-$(TRACKER_API_VERSION).pc

tracker-sparql-$(TRACKER_API_VERSION).pc: tracker-sparql.pc
	cp $< $@

tracker-extract-$(TRACKER_API_VERSION).pc: tracker-extract.pc
	cp $< $@

tracker-miner-$(TRACKER_API_VERSION).pc: tracker-miner.pc
	cp $< $@

CLEANFILES =                                           \
	$(autostart_DATA)                              \
	$(desktop_in_files)                            \
	tracker-sparql-$(TRACKER_API_VERSION).pc       \
	tracker-extract-$(TRACKER_API_VERSION).pc      \
	tracker-miner-$(TRACKER_API_VERSION).pc

EXTRA_DIST = $(desktop_in_in_files)
