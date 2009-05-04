# $Id$

APP = lang_query
SRC = lang_query

LIB = dbapi_sample_base$(STATIC) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xutil xncbi
STATIC_LIB = $(DBAPI_CTLIB) $(DBAPI_ODBC) ncbi_xdbapi_ftds $(FTDS_LIB) ncbi_xdbapi_ftds_odbc $(FTDS64_ODBC_LIB) $(LIB)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
STATIC_LIBS = $(SYBASE_LIBS) $(ODBC_LIBS) $(FTDS_LIBS) $(LIBS)

CHECK_CMD  = test_lang_query.sh
CHECK_COPY = test_lang_query.sh
CHECK_TIMEOUT = 400
