APP = blast2seq
SRC = blast2seq
LIB = xblast xobjutil xobjread $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = objects dbapi
