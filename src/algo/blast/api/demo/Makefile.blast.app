APP = blast
SRC = blast_app blast_input
LIB = xblast xnetblastcli xnetblast scoremat ncbi_xloader_blastdb \
      xalnutil blastdb xalnmgr xobjutil xobjread tables $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(NCBI_C_LIBPATH) -lblastapi -lncbitool \
       -lncbiobj $(NCBI_C_ncbi) $(ORIG_LIBS)

