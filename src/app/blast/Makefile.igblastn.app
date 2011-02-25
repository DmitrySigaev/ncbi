WATCHERS = camacho madden maning

APP = igblastn
SRC = igblastn_app blast_app_util
LIB_ = $(BLAST_INPUT_LIBS) $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

# De-universalize Mac builds to work around a PPC toolchain limitation
CFLAGS   = $(FAST_CFLAGS:ppc=i386)
CXXFLAGS = $(FAST_CXXFLAGS:ppc=i386) -g
LDFLAGS  = $(FAST_LDFLAGS:ppc=i386) -g
 
CPPFLAGS = $(ORIG_CPPFLAGS) -g
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin
