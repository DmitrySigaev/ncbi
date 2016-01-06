#########################################################################
# This file was originally generated from by shell script "new_project.sh"
#########################################################################
APP = test-reference_allele_fix
SRC = test-reference_allele_fix

LIB = variation_utils ncbi_xloader_genbank ncbi_xreader_id1 ncbi_xreader_id2 \
      ncbi_xreader_cache ncbi_xreader id1 id2 seqsplit variation xobjutil \
      xconnect xcompress $(SOBJMGR_LIBS)

LIBS =  $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)
LDFLAGS = $(ORIG_LDFLAGS) -pthread -L../../../../../lib/ 
CXXFLAGS = $(ORIG_CXXFLAGS)  

