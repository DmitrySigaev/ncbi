#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build object manager test application "test_objmgr"
#################################

REQUIRES = dbapi

APP = test_objmgr
SRC = test_objmgr test_helper
LIB = $(SOBJMGR_LIBS)

LIBS = $(ORIG_LIBS)

CHECK_CMD = test_objmgr
