APP = rpstblastn
SRC = rpstblastn_app blast_format data4xmlformat blast_app_util
LIB_ = $(BLAST_FORMATTER_LIBS) $(BLAST_LIBS) $(OBJMGR_LIBS) xalgowinmask
LIB = $(LIB_:%=%$(STATIC))

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS)
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin
