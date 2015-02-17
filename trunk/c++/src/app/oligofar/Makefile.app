# This file was originally generated by shell script "import_project.sh" (R121211)
# Mon Aug 25 13:17:22 EDT 2008

# NOTE: If you put variable settings here, they will OVERRIDE individual
# projects; Makefile.mk is the place for default settings.

include $(builddir)/Makefile.app
MAKEFILE = $(user_makefile)
ALL = rm-if-outdated
rm-if-outdated:
	@for l in $(LIB) ''; do \
	   test -n "$$l"  ||  break; \
	   test -f $(XAPP)  ||  break; \
	   for ll in $(import_root)/../lib/lib$$l.*; do \
	      if test $$ll -nt $(XAPP); then \
	         echo Rebuilding $(XAPP) against new `basename $$ll`; \
	         rm -f $(XAPP); \
	      fi; \
	   done; \
	done

# Original MD5 checksum: 9a2367052661c25b9273051a03ffb94f