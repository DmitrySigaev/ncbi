# $Id$

APP = odbc_sp_who_ftds64
SRC = odbc_sp_who_ftds64

LIB  = ncbi_xdbapi_$(ftds64)_odbc$(STATIC) $(FTDS64_ODBC_LIB) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS64_ODBC_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(FTDS64_INCLUDE) $(ODBC_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_COPY = run_sample_odbc.sh
CHECK_CMD = run_sample_odbc.sh odbc_sp_who_ftds64
