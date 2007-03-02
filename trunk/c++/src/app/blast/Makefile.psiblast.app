APP = psiblast
SRC = psiblast_app blast_format blast_app_util
LIB = blastinput $(BLAST_FORMATTER_LIBS) $(BLAST_LIBS) $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(top_srcdir)/src/algo/blast/api
#CPPFLAGS = $(ORIG_CPPFLAGS) -I../algo/blast/api
LIBS = $(CMPRS_LIBS)
REQUIRES = objects
