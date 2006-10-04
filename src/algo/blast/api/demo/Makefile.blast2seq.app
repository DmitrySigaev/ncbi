APP = blast2seq
SRC = blast2seq blast_input
LIB = xblast composition_adjustment xnetblastcli xnetblast \
      xalgodustmask seqdb blastdb scoremat \
      xobjsimple xobjutil xobjread creaders tables \
      xalgoblastdbindex $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects dbapi -Cygwin
