# $Id$

APP = gene_info_writer_unit_test
SRC = gene_info_writer_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LOCAL_LDFLAGS = -L. -L.. -L../../../../../lib
LDFLAGS = $(LOCAL_LDFLAGS) $(FAST_LDFLAGS)

LIB = gene_info_writer gene_info xobjutil xobjsimple seqdb $(OBJMGR_LIBS) $(BLAST_LIBS)

LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) \
       $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
       
REQUIRES = objects

CHECK_CMD = gene_info_writer_unit_test
CHECK_COPY = data
CHECK_AUTHORS = blastsoft

