# $Id$

APP = dbl_lang
SRC = dbl_lang

LIB  = ncbi_xdbapi_dblib dbapi_driver $(XCONNEXT) xconnect xncbi
LIBS = $(SYBASE_DBLIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Sybase DBLib

CHECK_COPY = dbl_lang.ini
CHECK_CMD = run_sybase_app.sh dbl_lang
