# $Id$

APP = test_ncbi_conn_stream
SRC = test_ncbi_conn_stream
LIB = xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_conn_stream.sh
CHECK_COPY = test_ncbi_conn_stream.sh
