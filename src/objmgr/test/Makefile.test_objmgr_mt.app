#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager test application "testobjmgr_mt"
#################################

APP = testobjmgr1_mt
SRC = testobjmgr1_mt
LIB = xobjmgr1 id1 seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xconnect xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = testobjmgr1_mt

REQUIRES = MT

