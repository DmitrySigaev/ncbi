# $Id$

APP = dbapi_cursor
SRC = dbapi_cursor

LIB  = dbapi_sample_base$(STATIC) $(DBAPI_CTLIB) $(DBAPI_ODBC) \
       ncbi_xdbapi_ftds $(FTDS_LIB) dbapi_driver$(STATIC) \
       $(XCONNEXT) xconnect xutil xncbi

LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(ODBC_LIBS) $(FTDS_LIBS) \
       $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_COPY = dbapi_cursor.ini

WATCHERS = ucko
