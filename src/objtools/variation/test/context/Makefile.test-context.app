#########################################################################
# This file was originally generated from by shell script "new_project.sh"
#########################################################################
APP = test-context
SRC = test-context

LIB = variation_utils ncbi_xloader_genbank ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xreader_cache ncbi_xreader seqsplit id1 id2 variation xobjutil $(SOBJMGR_LIBS) xconnect xcompress
LIBS =  $(DL_LIBS) $(CMPRS_LIBS)
LDFLAGS = -pthread -L../../../../../lib/ 