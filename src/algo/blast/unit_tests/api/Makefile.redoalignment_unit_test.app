# $Id$

APP = redoalignment_unit_test
SRC = redoalignment_unit_test 

CPPFLAGS = -DNCBI_MODULE=BLAST $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) -I$(srcdir)/../../api -I$(srcdir)/../../core
LIB = blast_unit_test_util test_boost $(BLAST_INPUT_LIBS) ncbi_xloader_blastdb_rmt \
    $(BLAST_LIBS) xobjsimple $(OBJMGR_LIBS:ncbi_x%=ncbi_x%$(DLL))
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = MT in-house-resources
CHECK_CMD = redoalignment_unit_test
CHECK_COPY = redoalignment_unit_test.ini data

WATCHERS = boratyng madden 
