#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager test application "test_objmgr_mt"
#################################

REQUIRES = dbapi

APP = test_objmgr_mt
SRC = test_objmgr_mt test_helper
LIB = xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio \
      general dbapi_driver xser xutil xconnect test_mt xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_objmgr_mt

