APP = hit_matrix.cgi
SRC = blast_hitmatrix cgi_hit_matrix

LIB_ = w_hit_matrix gui_glmesa w_gl w_wx w_data \
	   gui_graph gui_opengl gui_print gui_objutils gui_utils \
	   xalgoalignutil xalnmgr ximage xcgi xhtml \
	   entrez2cli entrez2 valerr gbseq entrezgene biotree \
	   xconnserv xqueryparse $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(OSMESA_LIBS) $(WXWIDGETS_LIBS) $(IMAGE_LIBS) $(CMPRS_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)


CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(WXWIDGETS_INCLUDE) $(OSMESA_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = MESA

