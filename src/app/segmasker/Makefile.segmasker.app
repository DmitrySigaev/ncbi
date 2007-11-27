# $Id$

REQUIRES = dbapi objects algo

ASN_DEP = seq

APP = segmasker
SRC = segmasker seg

LIB = xobjsimple seqmasks_io xalgowinmask $(BLAST_LIBS:%=%$(STATIC)) \
      $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

