#################################
# $Id$
# Author:  Eugen Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build datatool application
#################################

APP = datatool
OBJ = moduleset alexer aparser parser lexer type value module \
      main generate code
LIB = xser xncbi

CPPFLAGS = -I. $(ORIG_CPPFLAGS)
LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(ORIG_LIBS)
