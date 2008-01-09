# $Id$
#################################
# Build demo "compart"

APP = compart
SRC = compart em


LIB =  xalgoalignutil ncbi_xloader_blastdb \
       xblast composition_adjustment xalgodustmask xnetblastcli \
       seqdb blastdb xnetblast \
       scoremat xobjutil tables \
       $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
