# $Id$

APP = logs_replay
SRC = logs_replay
LIB = xconnserv xconnect xutil xncbi

LIBS = $(DL_LIBS) $(ORIG_LIBS)

#REQUIRES = MT
REQUIRES = MT Linux

WATCHERS = gouriano
