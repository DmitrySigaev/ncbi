# $Id$

APP = tds95_mutex1
SRC = mutex1

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS95_INCLUDE) -I$(srcdir)/.. $(ORIG_CPPFLAGS)
LIB      = tds_ftds95$(STATIC)
LIBS     = $(FTDS95_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD =

WATCHERS = ucko
