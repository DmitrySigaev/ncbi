#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager demo application "objmgr_demo"
#################################

REQUIRES = bdb objects BerkeleyDB

APP = objmgr_demo
SRC = objmgr_demo
LIB = bdb $(OBJMGR_LIBS) $(GENBANK_READER_ID1C_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(BERKELEYDB_LIBS) $(ORIG_LIBS)
