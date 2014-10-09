# $Id$

APP = unit_test_loc_edit
SRC = unit_test_loc_edit

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xunittestutil $(XFORMAT_LIBS) xalnmgr xobjutil valerr \
       gbseq submit tables xregexp $(PCRE_LIB) test_boost $(OBJMGR_LIBS) \
       $(OBJEDIT_LIBS)

LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY = 
CHECK_TIMEOUT = 3000

WATCHERS = bollin
