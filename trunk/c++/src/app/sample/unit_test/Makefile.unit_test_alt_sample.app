# $Id$

APP = unit_test_alt_sample
SRC = unit_test_alt_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = test_boost xncbi
PRE_LIBS = $(BOOST_TEST_LIBS)

REQUIRES = Boost.Test

# Uncomment next 2 lines to run automatically as part of "make check"
# CHECK_COPY = unit_test_alt_sample.ini
# CHECK_CMD =
### END COPIED SETTINGS
