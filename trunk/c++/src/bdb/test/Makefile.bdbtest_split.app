# $Id$

APP = bdbtest_split
SRC = test_bdb_split
LIB = $(BDB_CACHE_LIB) $(BDB_LIB) xncbi xutil
LIBS = $(BERKELEYDB_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)
