# $Id$

SRC = pcretest
APP = test_regexp

CFLAGS = $(ORIG_CFLAGS) -I$(includedir)/internal/regexp -I$(srcdir)/../

LIBS = -lregexp

CHECK_CMD = test_regexp.sh
CHECK_COPY = testdata test_regexp.sh
