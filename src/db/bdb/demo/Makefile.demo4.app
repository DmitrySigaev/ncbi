# $Id$

APP = bdb_demo4
SRC = demo4
LIB = bdb xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)

WATCHERS = kuznets
