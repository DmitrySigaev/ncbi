# $Id$

APP = bdb_demo3
SRC = demo3
LIB = bdb xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)

WATCHERS = kuznets
