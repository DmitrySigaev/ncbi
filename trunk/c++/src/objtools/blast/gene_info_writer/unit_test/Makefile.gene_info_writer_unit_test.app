# $Id$

APP = gene_info_writer_unit_test
SRC = gene_info_writer_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(LOCAL_LDFLAGS) $(FAST_LDFLAGS)

LIB_ = test_boost gene_info_writer gene_info xobjutil seqdb blastdb $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
REQUIRES = Boost.Test.Included

CHECK_CMD  = gene_info_writer_unit_test
CHECK_COPY = data
CHECK_AUTHORS = blastsoft

