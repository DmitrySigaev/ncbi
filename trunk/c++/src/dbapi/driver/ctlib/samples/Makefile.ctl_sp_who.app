# $Id$

APP = ctl_sp_who
SRC = ctl_sp_who

LIB  = ncbi_xdbapi_ctlib dbapi_driver $(XCONNEXT) xconnect xncbi
LIBS = $(SYBASE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Sybase

CHECK_COPY = ctl_sp_who.ini
CHECK_CMD = run_sybase_app.sh ctl_sp_who
