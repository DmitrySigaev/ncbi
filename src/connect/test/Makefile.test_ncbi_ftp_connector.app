# $Id$

APP = test_ncbi_ftp_connector
SRC = test_ncbi_ftp_connector
LIB = connect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD  =
