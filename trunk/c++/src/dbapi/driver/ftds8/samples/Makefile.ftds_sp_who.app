# $Id$

APP = ftds_sp_who
SRC = ftds_sp_who

LIB  = ncbi_xdbapi_ftds $(FTDS8_LIB) dbapi_driver xncbi
LIBS = $(FTDS8_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(FTDS8_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = FreeTDS

CHECK_CMD =
