#################################
# $Id$
#################################

APP = netcached
SRC = netcached message_handler smng_thread \
      nc_storage nc_storage_blob nc_db_files nc_db_stat nc_memory

REQUIRES = MT SQLITE3


LIB = xconnserv xthrserv xconnect xutil xncbi sqlitewrapp
LIBS = $(SQLITE3_STATIC_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(SQLITE3_INCLUDE) $(ORIG_CPPFLAGS)
