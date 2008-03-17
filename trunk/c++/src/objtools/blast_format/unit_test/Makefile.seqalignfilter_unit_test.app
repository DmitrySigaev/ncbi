# $Id$

APP = seqalignfilter_unit_test
SRC = seqalignfilter_unit_test seq_writer_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIB_ = $(BLAST_FORMATTER_LIBS) $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(CMPRS_LIBS) \
		$(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = seqalignfilter_unit_test
CHECK_AUTHORS = blastsoft
