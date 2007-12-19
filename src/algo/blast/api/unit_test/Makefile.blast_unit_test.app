# $Id$

APP = blast_unit_test
SRC = test_objmgr blast_test_util bl2seq_unit_test gencode_singleton_unit_test \
	blastoptions_unit_test blastfilter_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB_ = $(BLAST_LIBS) xobjsimple $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(NETWORK_LIBS) \
		$(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects

CHECK_CMD = blast_unit_test
CHECK_AUTHORS = blastsoft
