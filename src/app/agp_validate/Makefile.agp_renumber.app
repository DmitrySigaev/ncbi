#################################
# $Id$
#################################

REQUIRES = objects

APP = agp_renumber
SRC = agp_renumber

LIB = xobjread xidmapper seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xncbi
