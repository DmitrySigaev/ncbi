# $Id$

APP = unit_test_score_builder
SRC = unit_test_score_builder

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xalnmgr xobjutil submit tables test_boost $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

#CHECK_COPY = data

CHECK_CMD =

WATCHERS = grichenk
