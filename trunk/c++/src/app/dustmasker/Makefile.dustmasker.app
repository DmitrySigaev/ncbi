# $Id$

ASN_DEP = seq

APP = dustmasker
SRC = main dust_mask_app 

LIB = xalgodustmask \
      xblast xnetblastcli xnetblast scoremat seqdb blastdb tables \
      xobjread xobjutil $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

