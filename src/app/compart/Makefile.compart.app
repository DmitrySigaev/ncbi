# $Id$
#################################
# Build demo "compart"

APP = compart
SRC = compart

LIB =  xalgoalignsplign xalgoalignutil xalgoalignnw xqueryparse \
       $(BLAST_LIBS:%=%$(STATIC)) \
       $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = -Cygwin

WATCHERS = chetvern
