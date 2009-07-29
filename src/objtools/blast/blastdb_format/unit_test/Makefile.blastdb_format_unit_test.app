# $Id$

APP = blastdb_format_unit_test
SRC = seq_writer_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIB_ = test_boost $(BLAST_FORMATTER_LIBS) \
    $(BLAST_LIBS) $(OBJMGR_LIBS)

LIB = $(LIB_:%=%$(STATIC))
LIBS = $(PCRE_LIBS) \
    $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = blastdb_format_unit_test
CHECK_COPY = data

REQUIRES = Boost.Test.Included

CHECK_AUTHORS = blastsoft
