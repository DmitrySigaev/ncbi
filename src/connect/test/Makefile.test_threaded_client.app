# $Id$

APP = test_threaded_client
SRC = test_threaded_client
LIB = xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify -best-effort CC

# Neither of these can contain make variables. :-/
CHECK_CMD  = test_threaded_client_and_server.sh
CHECK_COPY = test_threaded_client_and_server.sh
