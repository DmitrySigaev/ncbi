# $Id$

APP = ctl_lang_ftds64
SRC = ctl_lang_ftds64 dbapi_driver_sample_base_ftds64

LIB  = ncbi_xdbapi_$(ftds64)$(STATIC) $(FTDS64_CTLIB_LIB) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS64_CTLIB_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = -I$(includedir)/dbapi/driver/ftds64 $(FTDS64_INCLUDE) $(ORIG_CPPFLAGS)

# CHECK_CMD = run_sybase_app.sh ctl_lang_ftds64
CHECK_CMD = run_sybase_app.sh ctl_lang_ftds64 -S MS_DEV1 -v 80
CHECK_CMD = run_sybase_app.sh ctl_lang_ftds64 -S SCHUMANN -v 42
CHECK_CMD = run_sybase_app.sh ctl_lang_ftds64 -S SCHUMANN -v 46
