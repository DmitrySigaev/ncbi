#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build object manager test application "test_objmgr"
#################################

REQUIRES = dbapi

APP = test_seqvector_ci
SRC = test_seqvector_ci
LIB = $(OBJMGR_LIBS)

LIBS = $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_seqvector_ci
CHECK_TIMEOUT = 500
