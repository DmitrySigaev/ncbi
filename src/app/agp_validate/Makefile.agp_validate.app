# $Id$

APP = agp_validate
SRC = agp_validate ContextValidator AltValidator MapCompLen

LIB = entrez2cli entrez2 taxon1 xobjutil xobjread \
      $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

