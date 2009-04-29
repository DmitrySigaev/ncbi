# $Id$

APP = blastfmt_unit_test
SRC = blast_hitmatrix_wrapper hitmatrix_unit_test

CPPFLAGS = $(OSMESA_INCLUDE) $(FLTK_INCLUDE) $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIB_ = $(BLAST_FORMATTER_LIBS) w_hit_matrix w_aln_data gui_glmesa w_gl \
      gui_graph gui_opengl gui_print gui_objutils gui_utils_fl gui_utils \
      xalgoalignutil xalnmgr ximage xcgi xhtml entrez2cli entrez2 valerr \
      gbseq xconnserv test_boost $(BLAST_LIBS) $(OBJMGR_LIBS)

LIB = $(LIB_:%=%$(STATIC))
LIBS = $(OSMESA_LIBS) $(IMAGE_LIBS) \
    $(FLTK_LIBS_ALL) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = blastfmt_unit_test
CHECK_COPY = data
CHECK_AUTHORS = blastsoft
CHECK_REQUIRES = PNG Boost.Test.Included
