#################################
# $Id$
#################################

APP = test_bulkinfo_mt
SRC = test_bulkinfo_mt
LIB = xobjutil $(OBJMGR_LIBS) test_mt ncbi_xdbapi_ftds $(FTDS64_CTLIB_LIB) dbapi_driver$(STATIC)

LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = bad_len.ids all_readers.sh

CHECK_CMD = all_readers.sh test_bulkinfo_mt -type gi /CHECK_NAME=test_bulkinfo_mt_gi
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type acc /CHECK_NAME=test_bulkinfo_mt_acc
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type label /CHECK_NAME=test_bulkinfo_mt_label
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type taxid /CHECK_NAME=test_bulkinfo_mt_taxid
CHECK_CMD = all_readers.sh -id2 test_bulkinfo_mt -type length -threads 8 /CHECK_NAME=test_bulkinfo_mt_length
CHECK_CMD = all_readers.sh -id2 test_bulkinfo_mt -type type -threads 8 /CHECK_NAME=test_bulkinfo_mt_type
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type state /CHECK_NAME=test_bulkinfo_mt_state
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type gi -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_gi
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type acc -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_acc
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type label -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_label
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type taxid -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_taxid
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type length -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_length
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type type -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_types
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type state -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_state

CHECK_TIMEOUT = 400

WATCHERS = vasilche
