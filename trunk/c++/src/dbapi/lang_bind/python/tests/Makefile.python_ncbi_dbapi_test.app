# $Id$

APP = python_ncbi_dbapi_test
SRC = python_ncbi_dbapi_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(PYTHON_INCLUDE) $(CPPUNIT_INCLUDE)

LIB  = xutil xncbi
LIBS = $(CPPUNIT_LIBS) $(PYTHON_LIBS) $(ORIG_LIBS)

CHECK_CMD = python_ncbi_dbapi_test.sh
CHECK_COPY = python_ncbi_dbapi_test.sh