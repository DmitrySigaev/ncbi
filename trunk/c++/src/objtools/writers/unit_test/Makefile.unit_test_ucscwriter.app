# $Id$

APP = unit_test_ucscwriter
SRC = unit_test_ucscwriter
LIB = xunittestutil xobjwrite variation_utils $(OBJREAD_LIBS) xobjutil gbseq xalnmgr entrez2cli entrez2 \
	tables test_boost $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD  =
CHECK_COPY = ucscwriter_test_cases

WATCHERS = gotvyans

