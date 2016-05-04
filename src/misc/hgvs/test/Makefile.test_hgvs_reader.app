# $Id$

APP = test_hgvs_reader

SRC = test_hgvs_reader

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) -DBOOST_ERROR_CODE_HEADER_ONLY \
           -DBOOST_SYSTEM_NO_DEPRECATED
CXXFLAGS = $(ORIG_CXXFLAGS)
LDFLAGS  = $(ORIG_LDFLAGS)

LIB = hgvs \
      $(OBJREAD_LIBS) xobjutil taxon1 \
      gencoll_client \
      entrez2cli entrezgene entrez2 xregexp $(PCRE_LIB) \
      test_boost $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD = test_hgvs_reader

WATCHERS = ludwigf meric
