#################################
# $Id$
# Author:  Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#################################

# Build test CoreLib application "cgidemo"
#################################

APP = cgidemo
OBJ = cgidemo
LIB = xncbi

LIBS = $(ORIG_LIBS) \
  -L$(NCBI_C_LIB) -lsocket -lnsl -lfcgi`sh -c 'CC -V 2>&1'|cut -f4 -d' '`
