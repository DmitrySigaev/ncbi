# $Id$

REQUIRES = dbapi

APP = alnvwr
SRC = alnvwr
LIB = xalnmgr submit $(OBJMGR_LIBS)

LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
