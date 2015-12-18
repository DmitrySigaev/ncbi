#################################
# $Id$
#################################

REQUIRES = dbapi FreeTDS

APP = test_objmgr_data
SRC = test_objmgr_data
LIB = $(OBJMGR_LIBS) ncbi_xdbapi_ftds $(FTDS_LIB) dbapi_driver$(STATIC)

LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = test_objmgr_data.sh test_objmgr_data_ids.sh test_objmgr_data.id1 test_objmgr_data.id2 test_objmgr_data.id_wgs1 test_objmgr_data.id_wgs2
CHECK_CMD = test_objmgr_data_ids.sh id2 test_objmgr_data
CHECK_CMD = test_objmgr_data.sh id1 /CHECK_NAME=test_objmgr_data_id1
CHECK_CMD = test_objmgr_data.sh id2 /CHECK_NAME=test_objmgr_data_id2
CHECK_CMD = test_objmgr_data.sh pubseqos /CHECK_NAME=test_objmgr_data_pubseqos
CHECK_CMD = test_objmgr_data -prefetch
CHECK_TIMEOUT = 800

WATCHERS = vasilche
