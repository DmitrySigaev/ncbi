# $Id$

APP = test_tar
SRC = test_tar
LIB = $(COMPRESS_LIBS) xutil xncbi
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = unix -WorkShop

CHECK_CMD = test_tar.sh
CHECK_COPY = test_tar.sh
CHECK_AUTHORS = lavr
CHECK_TIMEOUT = 400
