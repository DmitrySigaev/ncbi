#################################
# $Id$
# Author:  Eugen Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build datatool application
#################################

APP = datatool
OBJ = datatool \
	type namespace statictype enumtype reftype unitype blocktype choicetype \
	typestr ptrstr stdstr classstr enumstr stlstr choicestr choiceptrstr \
	value mcontainer module moduleset generate filecode code \
	fileutil alexer aparser parser lexer exceptions comments srcutil
LIB = xser xncbi xutil

CPPFLAGS = -I. $(ORIG_CPPFLAGS)
LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(MATH_LIBS) $(ORIG_LIBS)
