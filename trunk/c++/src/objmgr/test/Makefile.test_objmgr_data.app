#################################
# $Id$
#################################

REQUIRES = dbapi

APP = test_objmgr_data
SRC = test_objmgr_data
LIB = xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio general \
	dbapi_driver xser xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_objmgr_loaders.sh test_objmgr_data
CHECK_COPY = test_objmgr_loaders.sh
