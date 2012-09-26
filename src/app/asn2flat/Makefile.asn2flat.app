#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "asn2flat"
#################################

APP = asn2flat
SRC = asn2flat
LIB  = xcleanup xobjread $(XFORMAT_LIBS) xalnmgr xobjutil entrez2cli entrez2 creaders tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

WATCHERS = ludwigf
