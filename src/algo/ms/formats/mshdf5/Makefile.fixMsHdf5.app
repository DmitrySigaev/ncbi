#
# $Id$
#
#
#############################################################################
###  EDIT SETTINGS FOR THE DEFAULT (APPLICATION) TARGET HERE              ### 

REQUIRES = Xerces

APP = fixMsHdf5
SRC = fixMsHdf5
# OBJ =

# PRE_LIBS = $(NCBI_C_LIBPATH) .....

#LINK_WRAPPER = $(top_srcdir)/scripts/common/impl/favor-static

LIB = mshdf5 mzXML general xregexp $(PCRE_LIB) xconnect xser xutil xncbi xcompress
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(HDF5_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(XERCES_INCLUDE) $(HDF5_INCLUDE)

# CFLAGS   = $(ORIG_CFLAGS)
# CXXFLAGS = $(ORIG_CXXFLAGS)
LDFLAGS  =  $(ORIG_LDFLAGS) $(XERCES_LIBS) $(HDF5_LIBS)

#                                                                         ###
#############################################################################
