#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build object manager test application "testobjmgr"
#################################

REQUIRES = dbapi

APP = testobjmgr1
SRC = testobjmgr1 test_helper
LIB = xobjutil xobjmgr id1 submit seqset $(SEQ_LIBS) pub medline biblio \
      general dbapi_driver xser xutil xconnect xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = testobjmgr1
