#################################
# $Id$
#################################

WATCHERS = satskyse

APP = netscheduled
SRC = netscheduled queue_coll job_status queue_clean_thread \
      notif_thread ns_affinity ns_queue access_list ns_util ns_format \
      worker_node job ns_server_misc ns_server_params ns_js_request ns_handler \
      ns_server ns_queue_db_block ns_queue_parameters

REQUIRES = MT bdb Linux


LIB =  $(BDB_LIB) xconnserv xthrserv xconnect xqueryparse xutil xncbi
LIBS = $(BERKELEYDB_STATIC_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE) -DBMCOUNTOPT

