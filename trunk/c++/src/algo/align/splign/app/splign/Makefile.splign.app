#################################
# $Id$
#################################

APP = splign
SRC = splign_app


LIB = xalgoalignsplign xalgoalignutil xalgoalignnw xalgoseq \
      ncbi_xloader_blastdb ncbi_xloader_lds lds bdb \
      xalnmgr xregexp taxon1 \
      $(BLAST_LIBS:%=%$(STATIC)) \
      $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(BERKELEYDB_STATIC_LIBS) \
       $(NETWORK_LIBS) \
       $(DL_LIBS) \
       $(ORIG_LIBS)


CXXFLAGS = $(FAST_CXXFLAGS)
#CXXFLAGS = $(FAST_CXXFLAGS) -D GENOME_PIPELINE
#CXXFLAGS = $(FAST_CXXFLAGS) -D ALGOALIGN_NW_SPLIGN_MAKE_PUBLIC_BINARY

LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = BerkeleyDB objects -Cygwin
