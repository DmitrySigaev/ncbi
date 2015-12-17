# $Id$

APP = dbapi_context_test
SRC = dbapi_unit_test_common dbapi_context_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = ncbi_xdbapi_ftds ncbi_xdbapi_ftds64 $(FTDS64_CTLIB_LIB) \
       $(DBAPI_CTLIB) $(DBAPI_ODBC) dbapi$(STATIC) dbapi_driver$(STATIC) \
       $(XCONNEXT) xconnect xutil test_boost xncbi

LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(ODBC_LIBS) $(FTDS_LIBS) \
       $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_REQUIRES = in-house-resources

CHECK_COPY = dbapi_context_test.ini

CHECK_TIMEOUT = 60

CHECK_CMD = dbapi_context_test -dr ftds64     -S MsSql
CHECK_CMD = dbapi_context_test -dr odbc       -S MsSql
CHECK_CMD = dbapi_context_test -dr ctlib      -S Sybase
CHECK_CMD = dbapi_context_test -dr ftds64     -S Sybase

WATCHERS = ucko
