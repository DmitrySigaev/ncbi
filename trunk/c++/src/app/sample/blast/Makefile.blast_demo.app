#################################
# $Id$
#################################

APP = blast_demo
SRC = blast_demo

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
BLAST_INPUT_LIBS = blastinput xregexp $(PCRE_LIB)
LIB = $(BLAST_INPUT_LIBS) ncbi_xloader_blastdb $(BLAST_LIBS) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(PCRE_LIBS) $(DL_LIBS) $(ORIG_LIBS)

# These settings are necessary for optimized WorkShop builds, due to
# BLAST's own use of them.
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects -Cygwin
### END COPIED SETTINGS

