# $Id$

APP = test_grid_worker
SRC = test_grid_worker
LIB = ncbi_xblobstorage_netcache xconnserv xthrserv xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = test_grid_worker.ini
