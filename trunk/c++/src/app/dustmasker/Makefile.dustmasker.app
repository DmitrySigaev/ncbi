# $Id$

REQUIRES = dbapi objects algo

ASN_DEP = seq

APP = dustmasker
SRC = main dust_mask_app

LIB = xalgodustmask seqmasks_io xalgowinmask xobjread creaders xobjutil \
	$(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

