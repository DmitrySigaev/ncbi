# $Id$
#################################
# Build demo "compart"

APP = compart
SRC = compart

LIB = xalgoalignutil xalnmgr xobjutil tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
