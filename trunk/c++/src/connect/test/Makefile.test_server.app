# $Id$

APP = test_server
SRC = test_server
LIB = xthrserv xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify -best-effort CC

REQUIRES = MT

# Tested along with test_threaded_client from its makefile.
