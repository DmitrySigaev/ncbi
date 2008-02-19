#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "speedtest"
#################################

APP = speedtest
SRC = speedtest
LIB = xformat xobjutil gbseq xalnmgr xcleanup submit entrez2cli entrez2 tables $(SOBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

