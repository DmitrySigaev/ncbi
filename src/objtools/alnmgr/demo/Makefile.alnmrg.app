# $Id$


APP = alnmrg
SRC = alnmrg
LIB = xalnmgr submit tables $(OBJMGR_LIBS) xobjread xutil ncbi_xloader_blastdb seqdb blastdb

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD  = run_sybase_app.sh alnmrg.sh /CHECK_NAME=alnmrg.sh
CHECK_COPY = alnmrg.sh data
CHECK_AUTHORS = todorov