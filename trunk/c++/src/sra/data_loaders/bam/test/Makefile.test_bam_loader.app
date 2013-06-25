# $Id$

APP = test_bam_loader
SRC = test_bam_loader

REQUIRES = Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = ncbi_xloader_bam bamread $(BAM_LIBS) xobjreadex submit xobjutil test_boost $(OBJMGR_LIBS)

LIBS = $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_COPY = mapfile test_bam_loader.ini
CHECK_CMD = test_bam_loader
CHECK_REQUIRES = -AIX -BSD -Solaris

WATCHERS = vasilche ucko
