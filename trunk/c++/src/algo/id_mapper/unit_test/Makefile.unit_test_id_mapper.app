# $Id$

APP = unit_test_id_mapper

SRC = unit_test_id_mapper

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xid_mapper gencoll_client genome_collection seqset $(OBJREAD_LIBS) \
      xalnmgr $(XFORMAT_LIBS) xobjutil tables xregexp $(PCRE_LIB) test_boost $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Uncomment if you do not want it to run automatically as part of
# "make check".
#CHECK_CMD = unit_test_id_mapper
#CHECK_CMD = unit_test_gene_model -data-in alignments.asn -data-expected annotations.asn -seqdata-expected seqdata.asn -combined-data-expected combined_annot.asn -combined-with-omission-expected combined_with_omission.asn
#CHECK_COPY = alignments.asn annotations.asn seqdata.asn combined_annot.asn combined_with_omission.asn

WATCHERS = boukn meric
