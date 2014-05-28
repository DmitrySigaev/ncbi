# $Id$
#
# Makefile:  for C++ discrepancy report
# Author: J. Chen
#

###  BASIC PROJECT SETTINGS
APP = api_asndisc
SRC = cApiAsndisc

LIB = xdiscrepancy_report xvalidate xobjedit valid valerr \
        xmlwrapp \
        taxon3 $(XFORMAT_LIBS) xalnmgr xobjutil tables \
        macro xregexp $(PCRE_LIB) $(OBJREAD_LIBS) $(OBJMGR_LIBS) \
        $(ncbi_xreader_pubseqos2) $(OBJMGR_LIBS)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(PCRE_LIBS) \
       $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS= $(ORIG_CPPFLAGS) $(LIBXML_INCLUDE) $(LIBXSLT_INCLUDE)

REQUIRES = objects

WATCHERS = chenj bollin
