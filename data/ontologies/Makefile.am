include $(top_srcdir)/Makefile.decl

maemo_ontology =                                       \
	91-maemo.ontology   91-maemo.description

configdir = $(datadir)/tracker/ontologies

config_DATA =                                          \
	10-xsd.ontology	    10-xsd.description         \
	11-rdf.ontology	    11-rdf.description         \
	12-nrl.ontology	    12-nrl.description         \
	20-dc.ontology      20-dc.description          \
	30-nie.ontology	    30-nie.description         \
	31-nao.ontology	    31-nao.description         \
	90-tracker.ontology 90-tracker.description     \
	91-ivi.ontology	    91-ivi.description

if HAVE_MAEMO
config_DATA +=                                         \
	$(maemo_ontology)
endif

EXTRA_DIST =                                           \
	$(maemo_ontology)                              \
	$(config_DATA)

