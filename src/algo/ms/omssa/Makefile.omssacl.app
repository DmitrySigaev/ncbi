# $Id$
# Author:  Lewis Y. Geer

# Build application "omssacl"
#################################

# NCBI_C_LIBS =  -lncbitool -lblastcompadj -lncbiobj -lncbi

# LIBS = $(PCRE_LIBS) $(NCBI_C_LIBPATH) $(NCBI_C_LIBS) $(ORIG_LIBS)
LIBS = $(PCRE_LIBS) $(ORIG_LIBS)

# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)

APP = omssacl

SRC = omssacl

# LIB = xomssa omssa xser xregexp $(PCRE_LIB) xutil xncbi
LIB = xomssa omssa blast tables connect seqdb blastdb seqset seq seqcode \
 sequtil pub medline biblio general xser xregexp $(PCRE_LIB) xutil xncbi

CXXFLAGS = $(FAST_CXXFLAGS)

# static link
# LINK_WRAPPER = $(top_srcdir)/scripts/favor-static

LDFLAGS  = $(FAST_LDFLAGS)
