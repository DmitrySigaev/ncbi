# $Id$

APP = ftds_lang
SRC = ftds_lang

LIB  = ncbi_xdbapi_ftds$(STATIC) $(FTDS8_LIB) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS8_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(FTDS8_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = FreeTDS

CHECK_COPY = ftds_lang.ini
CHECK_CMD =
