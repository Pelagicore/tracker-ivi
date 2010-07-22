include $(top_srcdir)/Makefile.decl

modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/sparql-modules

INCLUDES =								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/src/libtracker-sparql				\
	-I$(top_builddir)/src						\
	$(WARN_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(GIO_CFLAGS)

if HAVE_DBUS_FD_PASSING
INCLUDES += -DHAVE_DBUS_FD_PASSING_IN_VALA=\"1\"
else
INCLUDES += -DHAVE_DBUS_FD_PASSING_IN_VALA=\"0\"
endif

modules_LTLIBRARIES = libtracker-bus.la

libtracker_bus_la_VALASOURCES = \
	tracker-bus.vala			\
	tracker-array-cursor.vala

libtracker_bus_la_SOURCES = 	\
	libtracker-bus.vala.stamp			\
	$(libtracker_bus_la_VALASOURCES:.vala=.c)	\
	tracker-bus-fd-cursor.c				\
	tracker-bus-fd-update.c				\
	tracker-bus-fd-update.h

libtracker_bus_la_LDFLAGS = 	\
	-module -avoid-version -no-undefined

noinst_HEADERS = 			\
	tracker-bus.h

# Vala sources
vapi_sources =						\
	config.vapi \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi	\
	$(top_srcdir)/src/libtracker-bus/tracker-bus-fd-cursor.vapi \
	$(top_srcdir)/src/libtracker-common/libtracker-common.vapi

libtracker-bus.vala.stamp: $(libtracker_bus_la_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 -H tracker-bus.h --vapi tracker-bus-$(TRACKER_API_VERSION).vapi tracker-bus-fd-update.vapi $^
	$(AM_V_GEN)touch $@

libtracker_bus_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(GLIB2_LIBS) \
	$(DBUS_LIBS) \
	$(GIO_LIBS)

BUILT_SOURCES = 					\
	libtracker-bus.vala.stamp

CLEANFILES = 						\
	$(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	$(libtracker_bus_la_VALASOURCES:.vala=.c) \
	libtracker-bus.vala.stamp			\
	tracker-bus.h

EXTRA_DIST = 						\
	config.vapi \
	$(libtracker_bus_la_VALASOURCES) \
	libtracker-bus.vala.stamp \
	$(top_srcdir)/src/libtracker-bus/tracker-bus-fd-cursor.vapi