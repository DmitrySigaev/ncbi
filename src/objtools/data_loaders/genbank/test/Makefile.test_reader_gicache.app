# $Id$

REQUIRES = unix

APP = test_reader_gicache
SRC = test_reader_gicache
LIB = ncbi_xreader_gicache $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

//CHECK_CMD = test_reader_gicache

WATCHERS = vasilche
