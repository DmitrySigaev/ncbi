#! /bin/sh
# $Id$
# Authors:  Denis Vakatov (vakatov@ncbi.nlm.nih.gov),
#           Aaron Ucko    (ucko@ncbi.nlm.nih.gov)

def_builddir="$NCBI/c++/Debug/build"

script=`basename $0`


#################################

Usage()
{
  cat <<EOF
USAGE:  $script <name> <type> [builddir]
SYNOPSIS:
   Create a model makefile "Makefile.<name>_<type>" to build
   a library or an application that uses pre-built NCBI C++ toolkit.
   Also include sample code when creating applications.
ARGUMENTS:
   <name>      -- name of the project (will be subst. to the makefile name)
   <type>      -- one of the following:
                  lib         to build a library
                  app[/basic] to build a simple application
                  app/cgi     to build a CGI or FastCGI application
                  app/dbapi   to build a DBAPI application
                  app/gui     to build an FLTK application
                  app/objects to build an application using ASN.1 objects
                  app/objmgr  to build an application using the object manager
                  app/alnmgr  to build an application using the alignment mgr.
   [builddir]  -- path to the pre-built NCBI C++ toolkit
                  (default = $def_builddir)

EOF

  test -z "$1"  ||  echo ERROR:  $1
  exit 1
}

#################################

CreateMakefile_Lib()
{
  makefile_name="$1"
  proj_name="$2"

  cat > "$makefile_name" <<EOF
#
# Makefile:  $user_makefile
#
# This file was originally generated by shell script "$script"
# ${timestamp}
#


###  PATH TO A PRE-BUILT C++ TOOLKIT  ###
builddir = $builddir
# builddir = \$(NCBI)/c++/Release/build


###  DEFAULT COMPILATION FLAGS -- DON'T EDIT OR MOVE THESE 5 LINES !!!
include \$(builddir)/Makefile.mk
srcdir = .
BINCOPY = @:
BINTOUCH = @:
LOCAL_CPPFLAGS = -I.


#############################################################################
###  EDIT SETTINGS FOR THE DEFAULT (LIBRARY) TARGET HERE                  ### 

LIB = $proj_name
SRC = $proj_name
# OBJ =

# CPPFLAGS = \$(ORIG_CPPFLAGS) \$(NCBI_C_INCLUDE)
# CFLAGS   = \$(ORIG_CFLAGS)
# CXXFLAGS = \$(ORIG_CXXFLAGS)
#
# LIB_OR_DLL = dll
#                                                                         ###
#############################################################################


###  LIBRARY BUILD RULES -- DON'T EDIT OR MOVE THESE 2 LINES !!!
include \$(builddir)/Makefile.is_dll_support
include \$(builddir)/Makefile.\$(LIB_OR_DLL)


###  PUT YOUR OWN ADDITIONAL TARGETS (MAKE COMMANDS/RULES) BELOW HERE
EOF
}



#################################

CreateMakefile_App()
{
  makefile_name="$1"
  proj_name="$2"
  proj_subtype="$3"

  cat > "$makefile_name" <<EOF
#
# Makefile:  $user_makefile
#
# This file was originally generated by shell script "$script"
# ${timestamp}
#


###  PATH TO A PRE-BUILT C++ TOOLKIT
builddir = $builddir
# builddir = \$(NCBI)/c++/Release/build


###  DEFAULT COMPILATION FLAGS  -- DON'T EDIT OR MOVE THESE 4 LINES !!!
include \$(builddir)/Makefile.mk
srcdir = .
BINCOPY = @:
LOCAL_CPPFLAGS = -I.


#############################################################################
###  EDIT SETTINGS FOR THE DEFAULT (APPLICATION) TARGET HERE              ### 
APP = $proj_name
SRC = $proj_name
# OBJ =

# PRE_LIBS = \$(NCBI_C_LIBPATH) .....

EOF

  copying=false
  while read line; do
    case "$line" in
      "### BEGIN COPIED SETTINGS") copying=true ;;
      "### END COPIED SETTINGS"  ) break        ;;
      *)                           test $copying = true && echo "$line" ;;
    esac
  done < $src/app/sample/${proj_subtype}/Makefile.${proj_subtype}_sample.app \
      >> "$makefile_name"

  cat >> "$makefile_name" <<EOF

# CFLAGS   = \$(ORIG_CFLAGS)
# CXXFLAGS = \$(ORIG_CXXFLAGS)
# LDFLAGS  = \$(ORIG_LDFLAGS)
#                                                                         ###
#############################################################################


###  APPLICATION BUILD RULES  -- DON'T EDIT OR MOVE THIS LINE !!!
include \$(builddir)/Makefile.app


###  PUT YOUR OWN ADDITIONAL TARGETS (MAKE COMMANDS/RULES) HERE
EOF
}


Capitalize()
{
  FIRST=`echo $1 | sed -e 's/^\(.\).*/\1/' | tr '[a-z]' '[A-Z]'`
  rest=`echo $1 | sed -e 's/^.//'`
  echo "${FIRST}${rest}" | sed -e 's/[^a-zA-Z0-9]/_/g'
}


#################################

case $# in
  3)  proj_name="$1" ; proj_type="$2" ; builddir="$3" ;;
  2)  proj_name="$1" ; proj_type="$2" ; builddir="$def_builddir" ;;
  0)  Usage "" ;;
  *)  Usage "Invalid number of arguments" ;;
esac

if test ! -d "$builddir"  ||  test ! -f "$builddir/../inc/ncbiconf.h" ; then
  Usage "Pre-built NCBI C++ toolkit is not found in:  \"$builddir\""
fi
builddir=`(cd "${builddir}" ; pwd)`

src=`sed -ne 's:^top_srcdir *= *\([^ ]*\):\1/src:p' < $builddir/Makefile.mk \
     | head -1`
test -n $src || src=${NCBI}/c++/src

case "${proj_type}" in
  app/*)
    proj_subtype=`echo ${proj_type} | sed -e 's@^app/@@'`
    proj_type=app
    if test ! -d $src/app/sample/$proj_subtype; then
      Usage "Unsupported application type ${proj_subtype}"
    fi
    ;;
  app)
    proj_subtype=basic
    ;;
esac

if test "$proj_name" != `basename $proj_name` ; then
  Usage "Invalid project name:  \"$proj_name\""
fi

makefile_name="Makefile.${proj_name}_${proj_type}"
if test ! -d $proj_name ; then
  touch tmp$$
  if test ! -f ../$proj_name/tmp$$ ; then
     mkdir $proj_name
  fi
  rm -f tmp$$
fi
test -d $proj_name &&  makefile_name="$proj_name/$makefile_name"
makefile_name=`pwd`/$makefile_name

if test -f $makefile_name ; then
  echo "\"$makefile_name\" already exists.  Do you want to override it?  [y/n]"
  read answer
  case "$answer" in
    [Yy]*) ;;
    *    ) exit 2 ;;
  esac
fi


case "$proj_type" in
  lib )  CreateMakefile_Lib $makefile_name $proj_name ;;
  app )  CreateMakefile_App $makefile_name $proj_name $proj_subtype ;;
  * )  Usage "Invalid project type:  \"$proj_type\"" ;;
esac

echo "Created a model makefile \"$makefile_name\"."

if test "$proj_type" != "app"; then
  exit 0
fi


old_class_name=CSample`Capitalize ${proj_subtype}`Application
new_class_name=C`Capitalize ${proj_name}`Application

old_proj_name=${proj_subtype}_sample

old_dir=${src}/app/sample/${proj_subtype}
if test -d "${proj_name}"; then
  new_dir=`pwd`/$proj_name
else
  new_dir=`pwd`
fi

for input in $old_dir/*; do
  base=`basename $input | sed -e "s/${old_proj_name}/${proj_name}/g"`
  case $base in
    *~ | CVS | Makefile.*) continue ;; # skip
  esac

  output=$new_dir/$base
  if test -f $output ; then
    echo "\"$output\" already exists.  Do you want to override it?  [y/n]"
    read answer
    case "$answer" in
      [Yy]*) ;;
      *    ) exit 3 ;;
    esac
  fi

  sed -e "s/${old_proj_name}/${proj_name}/g" \
      -e "s/${old_class_name}/${new_class_name}/g" < $input > $output
  
  echo "Created a model source file \"$output\"."
done
