# $Id$

APP = sqlite3_sample01
SRC = sqlite3_sample01

LIB  = ncbi_xdbapi_sqlite3$(STATIC) dbapi_driver$(STATIC) xncbi

LIBS = $(SQLITE3_LIBS) $(DL_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(SQLITE3_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_CMD = sqlite3_sample01 -S ./test.sqlite3 /CHECK_NAME=sqlite3_sample01

WATCHERS = ivanovp
