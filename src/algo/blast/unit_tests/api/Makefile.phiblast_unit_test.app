# $Id$

APP = phiblast_unit_test
SRC = phiblast_unit_test 

CPPFLAGS = -DNCBI_MODULE=BLAST $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) -I$(srcdir)/../../api
LIB = test_boost $(BLAST_LIBS) xobjsimple $(OBJMGR_LIBS:ncbi_x%=ncbi_x%$(DLL))
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = MT in-house-resources
CHECK_CMD = phiblast_unit_test
CHECK_COPY = phiblast_unit_test.ini

WATCHERS = boratyng madden camacho fongah2
