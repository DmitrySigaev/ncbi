#!/bin/sh
#
# $Id$
# Set some "domestic" paths, then run "configure"
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#############################################################################

USR_BUILD_DIR="$1"
DEF_BUILD_DIR="egcs_linux"

set -a

PM_PATH="/net/neptune/pubmed1/Cvs5"
NCBI_C_INCLUDE="/export/home/coremake/ncbi/include"
NCBI_C_LIB="/export/home/coremake/ncbi/altlib"

sh configure --exec_prefix=${USR_BUILD_DIR:=$DEF_BUILD_DIR}
