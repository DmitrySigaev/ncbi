#################################
# $Id$
#################################

APP = netscheduled
SRC = netscheduled bdb_queue job_status queue_clean_thread notif_thread \
      job_time_line

REQUIRES = MT bdb


LIB =  $(BDB_LIB) xconnserv-static xthrserv xconnect xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE) -DBMCOUNTOPT
