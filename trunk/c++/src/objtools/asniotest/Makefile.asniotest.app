# $Id$
# Author:  Paul Thiessen

# Build application "asniotest"
#################################

APP = asniotest
SRC = asniotest

LIB = \
    ncbimime \
    cdd \
    scoremat \
	cn3d \
	mmdb \
	id1cli id1 \
	seqset $(SEQ_LIBS) \
	pub \
	medline \
	biblio \
	general \
	xser \
	xconnect \
	xutil \
	xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD =
CHECK_COPY = data
CHECK_TIMEOUT = 300
