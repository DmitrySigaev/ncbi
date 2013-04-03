#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "asnval"
#################################

APP = asnvalidate
SRC = asnval
LIB = xvalidate xcleanup $(XFORMAT_LIBS) xalnmgr xobjutil \
      valid valerr submit taxon3 gbseq \
      tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = bollin
