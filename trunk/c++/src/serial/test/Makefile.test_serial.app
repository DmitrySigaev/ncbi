#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = test_serial
SRC = serialobject serialobject_Base test_serial cppwebenv twebenv \
      we_cpp__ we_cpp___
LIB = xcser xser xutil xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE) -I$(srcdir) -I$(srcdir)/webenv

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(ORIG_LIBS)

CHECK_CMD = test_serial.sh
CHECK_COPY = test_serial.sh
