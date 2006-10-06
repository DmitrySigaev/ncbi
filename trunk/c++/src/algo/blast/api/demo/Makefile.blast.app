APP = blast
SRC = blast_app blast_input
LIB = xblastformat ncbi_xloader_blastdb xalnmgr $(BLAST_LIBS) $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
NCBI_C_LIBS = -lblastapi -lncbitool -lncbiobj -lncbi
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects C-Toolkit
