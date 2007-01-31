#! /bin/sh

# $Id$
#
# Author:     Denis Vakatov,    NCBI
# MSVC stuff: Anton Lavrentiev, NCBI
# SVN stuff:  Dmitry Kazimirov, NCBI
#
#  Checkout everything needed to build the very basic (and portable)
#  NCBI C++ libraries and applications for all platforms, or just for the
#  specified one.

REPOS='https://svn.ncbi.nlm.nih.gov/repos/toolkit/trunk/c++'

##  Cmd.-line args
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`
target_dir=$1

checkout_info="checkout_info"
need_cleanup="no"
timestamp=`date -u +'%Y-%m-%d %H:%M:%S +0000'` # %z isn't portable :-/


##  Printout USAGE info, cleanup and exit
Usage() {
  cat <<EOF 1>&2
Usage:  $script_name <dir> [--export] ... [--<platform>] [--<srctree>]
Synopsis:
   Checkout a portable part of the NCBI C++ tree.
Arguments:
   <dir>             -- path to checkout the tree to
   --export          -- get a clean source tree without .svn directories
   --with-objects    -- generate ASN.1 serialization code in the "objects/" dir
   --without-objects -- do not even checkout "objects/" dirs
   --without-ctools  -- do not checkout projects that depend on the C Toolkit
   --without-gui     -- do not checkout projects that depend on FLTK
   --with-internal   -- checkout allowed internal code in the "internal/" dir
   --date=<date>     -- checkout as at the start of the date (`date`)
   --<platform>      -- "--unix", "--msvc", "--cygwin", "--mac", "--all"
   --<srctree>       -- "--production", "--development"
Default:
   $script_name <dir> --date=<current> --all --development

ERROR:  $1
EOF

  if test "$need_cleanup" = "yes" ; then
    cd "$target_dir"/..  &&  rm -rf "$target_dir"
  fi

  kill $$
  exit 1
}


##  Parse cmd.-line args
test $# -gt 0  ||  Usage "Too few arguments (target dir name is required)"
case "$target_dir" in
  -* )  Usage "Must specify target directory (name cannot start with '-')" ;;
esac

shift

export="no"
with_objects="checkout_only"
with_ctools=""
with_gui=""
with_internal="no"
platform="all"
srctree="development"

for cmd_arg in "$@" ; do
  case "$cmd_arg" in
    --export          )  export="yes" ;;

    --with-objects    )  with_objects="yes" ;;
    --without-objects )  with_objects="no"  ;;

    --with-ctools     )  with_ctools="yes" ;;
    --without-ctools  )  with_ctools="no"  ;;

    --with-gui        )  with_gui="yes" ;;
    --without-gui     )  with_gui="no"  ;;

    --with-internal   )  with_internal="yes" ;;
    --without-internal)  with_internal="no"  ;;

    --unix            )  platform="unix"   ;;
    --msvc            )  platform="msvc"   ;;
    --cygwin          )  platform="cygwin" ;;
    --mac             )  platform="mac"    ;;
    --all             )  platform="all"    ;;

    --production      )  srctree="production" ;;
    --development     )  srctree="development" ;;
    --date=*          )  timestamp="`echo $cmd_arg | sed -e 's/--date=//'`" ;;

    * )  Usage "Invalid command line argument:  $cmd_arg" ;;
  esac
done 

revision="{$timestamp}"
svn_cmd='svn --non-interactive'

# Checkout <path> [-N]
if test "$export" = "yes" ; then
    checkout_cmd="$svn_cmd export -r \"$revision\" $REPOS/\$1 \$1 \$2"
else
    checkout_cmd="$svn_cmd update -r \"$revision\" \$*"
fi


# This function calls with 2 arguments: <path> [-N]
# If you change number of arguments please update checkout_cmd above
Checkout()
{
    echo " $*..."
    eval $checkout_cmd    ||  Usage "$checkout_cmd"
}


if test -d "$target_dir"  ||  test -f "$target_dir" ; then
    Usage "\"$target_dir\" already exists"
fi


##  Checkout
echo 'Retrieving source files from Subversion:'

## Initial checkout
$svn_cmd checkout -N -r "$revision" $REPOS $target_dir || \
	Usage "Cannot checkout $REPOS"
need_cleanup="yes"

cd "$target_dir"  ||  Usage "Cannot cd to $target_dir"
target_dir=`pwd`

if test "$srctree" = "development" ; then
  srctree= # actual name in the repository
  Checkout ${srctree}doc
fi

Checkout ${srctree}src -N
Checkout ${srctree}src/connect -N
Checkout ${srctree}src/connect/ext
Checkout ${srctree}src/connect/services
Checkout ${srctree}src/connect/test
Checkout ${srctree}src/corelib
Checkout ${srctree}src/util
Checkout ${srctree}src/cgi
Checkout ${srctree}src/html
Checkout ${srctree}src/misc
Checkout ${srctree}src/serial
Checkout ${srctree}src/app
Checkout ${srctree}src/algo
Checkout ${srctree}src/bdb
Checkout ${srctree}src/sqlite
if test "$with_internal" != "no" ; then
    Checkout ${srctree}src/internal -N
    Checkout ${srctree}src/internal/align_model 
    Checkout ${srctree}src/internal/ID -N
    Checkout ${srctree}src/internal/ID/utils -N
    Checkout ${srctree}src/internal/log
fi

Checkout ${srctree}include -N
Checkout ${srctree}include/connect -N
Checkout ${srctree}include/connect/ext
Checkout ${srctree}include/connect/services
Checkout ${srctree}include/corelib
Checkout ${srctree}include/util
Checkout ${srctree}include/cgi
Checkout ${srctree}include/html
Checkout ${srctree}include/misc
Checkout ${srctree}include/serial
Checkout ${srctree}include/app
Checkout ${srctree}include/test
Checkout ${srctree}include/algo
Checkout ${srctree}include/bdb
Checkout ${srctree}include/sqlite
if test "$with_internal" != "no" ; then
    Checkout ${srctree}include/internal -N
    Checkout ${srctree}include/internal/align_model 
    Checkout ${srctree}include/internal/ID -N
    Checkout ${srctree}include/internal/ID/utils -N
    Checkout ${srctree}include/internal/log
fi

case "$platform" in
  all )
    Checkout ${srctree}src/dbapi
    Checkout ${srctree}include/dbapi
    Checkout ${srctree}src/connect/daemons
    Checkout ${srctree}include/connect/daemons
    Checkout ${srctree}compilers
    Checkout ${srctree}src/check
    ;;
  unix )
    Checkout ${srctree}src/dbapi
    Checkout ${srctree}include/dbapi
    Checkout ${srctree}src/connect/daemons
    Checkout ${srctree}include/connect/daemons
    Checkout ${srctree}compilers -N
    Checkout ${srctree}src/check
    ;;
  msvc )
    Checkout ${srctree}src/dbapi
    Checkout ${srctree}include/dbapi
    Checkout ${srctree}compilers -N
    Checkout ${srctree}compilers/msvc710_prj
    Checkout ${srctree}compilers/msvc800_prj
    Checkout ${srctree}src/check
    ;;
  cygwin)
    Checkout ${srctree}src/dbapi
    Checkout ${srctree}include/dbapi
    Checkout ${srctree}compilers -N
    Checkout ${srctree}compilers/cygwin
    Checkout ${srctree}src/check
    ;;
  mac )
    Checkout ${srctree}src/dbapi -N
    Checkout ${srctree}src/dbapi/driver -N
    Checkout ${srctree}include/dbapi -N
    Checkout ${srctree}include/dbapi/driver -N
    Checkout ${srctree}include/dbapi/driver/util -N
    Checkout ${srctree}src/connect/mitsock
    Checkout ${srctree}compilers -N
    Checkout ${srctree}compilers/mac_prj
    Checkout ${srctree}compilers/xCode
    ;;
esac

case "$platform" in
  all | unix | cygwin | mac )
    Checkout ${srctree} -N
    Checkout ${srctree}scripts
    ;;
  msvc )
    Checkout ${srctree}scripts -N
    Checkout ${srctree}scripts/check
    Checkout ${srctree}scripts/projects
    ;;
esac

if test "$with_objects" != "no" ; then
    Checkout ${srctree}src/objects
    Checkout ${srctree}src/objmgr
    Checkout ${srctree}src/objtools
    Checkout ${srctree}include/objects
    Checkout ${srctree}include/objmgr
    Checkout ${srctree}include/objtools
fi

if test "$with_ctools" != "no" ; then
    Checkout ${srctree}src/ctools
    Checkout ${srctree}include/ctools
fi

if test "$with_gui" != "no" ; then
    Checkout ${srctree}src/gui
    Checkout ${srctree}include/gui
fi


## Generate serialization code from ASN.1 specs
if test "$with_objects" = "yes" ; then
    echo
    echo 'Generating serialization code from ASN.1 specs:'
    cd src/objects  ||  Usage "Failed:  cd src/objects"
    { make builddir=$NCBI/c++.metastable/Release/build  ||  Usage "Failed to generate serialization classes" ; } \
    | grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.asn ' \
    | sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.asn\).*%  \1%g'
    cd ../../src/app/sample/soap   ||  Usage "Failed:  cd ../../src/app/sample/soap (from src/objects)" ;
    { $NCBI/c++.metastable/Release/build/new_module.sh --dtd soap_dataobj  ||  Usage "Failed to generate serialization classes" ; } \
        | grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.dtd ' \
        | sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.dtd\).*%  \1%g'
    cd ../../../..  ||  Usage "Failed:  cd ../../../..  (from src/app/sample/soap)"
    if test "$with_gui" != "no"; then
        cd src/gui/core  ||  Usage "Failed:  cd ../core (from src/gui/config)"
        for x in *.asn; do
            { $NCBI/c++.metastable/Release/build/new_module.sh `basename $x .asn`  ||  Usage "Failed to generate serialization classes" ; } \
                | grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.asn ' \
                | sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.asn\).*%  \1%g'
        done
        cd ../dialogs/edit/feature  ||  Usage "Failed:  cd ../dialogs/edit/feature (from src/gui/core)"
        { $NCBI/c++.metastable/Release/build/new_module.sh  ||  Usage "Failed to generate serialization classes" ; } \
            | grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.asn ' \
            | sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.asn\).*%  \1%g'
        cd ../../../../..  ||  Usage "Failed:  cd ../../../../..  (from src/gui/dialogs/edit/feature)"
    fi
fi


## Strip some unused stuff
case "$platform" in
  msvc )
    # UNIX-only sources
    rm -f src/connect/ncbi_lbsmd.c src/connect/ncbi_lbsm.c src/connect/ncbi_lbsm_ipc.c \
          include/connect/ext/ncbi_ifconf.h src/connect/ext/ncbi_ifconf.c \
          src/connect/ext/test/test_ncbi_ifconf.c \
          src/cgi/fcgi_run.cpp src/cgi/fcgibuf.cpp
    # scripts and aux. files (mostly "objects/"-related) 
    if test "$with_objects" = "yes" ; then
        rm -f src/objects/add_asn.sh \
              src/objects/nt_sources.sh src/objects/nt_sources.cmd
    fi
    ;;
esac
case "$platform" in
  msvc | mac )
    rm -f Makefile.* config* .psrc aclocal.m4 install-sh
    rm -f compilers/*.sh compilers/*.awk
    ;;
esac


## Strip root .svn for --export
if test "$export" = "yes" ; then
    rm -rf .svn
fi


## DONE

# Generate checkout info file
cat <<EOF >$checkout_info
For platform   : ${platform}
Sources date   : ${timestamp}
Checkout date  : `date`
Is exported    : ${export}
ASN generation : ${with_objects}
CTools         : ${with_ctools:-yes}
GUI            : ${with_gui:-yes}
EOF

cat <<EOF
---------------------------------
The core NCBI C++ toolkit tree has been successfully deployed:

`cat $checkout_info`
In directory   : ${target_dir}
Source tree    : ${srctree}
EOF
