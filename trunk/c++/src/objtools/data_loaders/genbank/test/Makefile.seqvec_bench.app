#################################
# $Id$
#################################

REQUIRES = dbapi

APP = seqvec_bench
SRC = seqvec_bench
LIB = test_mt $(OBJMGR_LIBS)

LIBS = $(DL_LIBS) $(ORIG_LIBS)
