# $Id$

APP = test_ncbi_http_get
SRC = test_ncbi_http_get

CPPFLAGS = $(GNUTLS_INCLUDE) $(ORIG_CPPFLAGS)

LIB = connssl connect $(NCBIATOMIC_LIB)
LIBS = $(GNUTLS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_http_get.sh
CHECK_COPY = test_ncbi_http_get.sh ../../check/ncbi_test_data

WATCHERS = lavr
