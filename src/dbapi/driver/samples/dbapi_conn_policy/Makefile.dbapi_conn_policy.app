# $Id$

APP = dbapi_conn_policy
SRC = dbapi_conn_policy

LIB  = dbapi_sample_base dbapi_driver $(XCONNEXT) xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CHECK_COPY = dbapi_conn_policy.ini
