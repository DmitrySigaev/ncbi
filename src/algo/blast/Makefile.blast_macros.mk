#################################
# $Id$
# This file contains macro definitions for using libraries maintained by the
# BLAST TEAM
# Author:  Christiam Camacho (camacho@ncbi.nlm.nih.gov)
#################################


BLAST_FORMATTER_MINIMAL_LIBS = xblastformat align_format blastdb_format \
    gene_info xalnmgr blastxml xcgi xhtml
# BLAST_FORMATTER_LIBS = $(BLAST_FORMATTER_MINIMAL_LIBS)
BLAST_FORMATTER_LIBS = $(BLAST_INPUT_LIBS)
BLAST_DB_DATA_LOADER_LIBS = ncbi_xloader_blastdb ncbi_xloader_blastdb_rmt
BLAST_INPUT_LIBS = blastinput xregexp $(PCRE_LIB) \
    $(BLAST_DB_DATA_LOADER_LIBS) $(BLAST_FORMATTER_MINIMAL_LIBS)

# BLAST_FORMATTER_LIBS and BLAST_INPUT_LIBS need $BLAST_LIBS
BLAST_LIBS = xblast xalgoblastdbindex composition_adjustment xalgodustmask \
             xalgowinmask seqdb blast_services xobjutil xobjread creaders \
             xnetblastcli xnetblast blastdb scoremat tables
# BLAST additionally needs xconnect $(SOBJMGR_LIBS) or $(OBJMGR_LIBS)
