#################################
# $Id$
# Author:  Clifford Clausen (clausen@ncbi.nlm.nih.gov)
#################################

# Build test application "test_regex"
#################################
APP = test_regexp
SRC = test_regexp
LIB = xregexp $(PCRE_LIB) xutil xncbi
LIBS = $(PCRE_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(FLTK_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_CMD = test_regexp
