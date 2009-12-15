# $Id$

APP = blastfmt_unit_test
SRC = blast_hitmatrix_wrapper hitmatrix_unit_test

CPPFLAGS = $(OSMESA_INCLUDE) $(WXWIDGETS_INCLUDE) $(ORIG_CPPFLAGS) \
		   $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIB_ = $(BLAST_FORMATTER_LIBS) w_hit_matrix gui_glmesa w_gl w_wx w_data \
      gui_graph gui_opengl gui_print gui_objutils gui_utils \
      xalgoalignutil xalnmgr ximage xcgi xhtml \
	  entrez2cli entrez2 valerr biotree gbseq entrezgene \
	  xconnserv xqueryparse test_boost $(BLAST_LIBS) $(OBJMGR_LIBS)

LIB = $(LIB_:%=%$(STATIC))
LIBS = $(OSMESA_LIBS) $(WXWIDGETS_LIBS) $(IMAGE_LIBS) \
	   $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = blastfmt_unit_test
CHECK_COPY = data

WATCHERS = camacho
