# $Id$
#################################

APP = bma_refiner

SRC = bma_refiner

REQUIRES = objects C-Toolkit

LIB =   xbma_refiner \
        xcd_utils ncbimime taxon1 \
        xstruct_util \
        xstruct_dp \
        xblast composition_adjustment seqdb blastdb xnetblastcli xnetblast \
        tables xobjsimple xobjutil \
        cdd \
        scoremat \
        cn3d \
        mmdb \
        $(OBJMGR_LIBS)

CXXFLAGS   = $(FAST_CXXFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(srcdir)/..

LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
