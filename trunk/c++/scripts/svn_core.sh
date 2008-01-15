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

REPOS_DEVELOPMENT='https://svn.ncbi.nlm.nih.gov/repos/toolkit/trunk/c++'
REPOS_PRODUCTION='https://svn.ncbi.nlm.nih.gov/repos/toolkit/production/current/c++'
GUI_URL='https://svn.ncbi.nlm.nih.gov/repos/gbench/branches/c++-toolkit'
REPOS=$REPOS_DEVELOPMENT

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


if test "$srctree" = "production" ; then
    REPOS=$REPOS_PRODUCTION
fi
revision="{$timestamp}"
svn_cmd='svn --non-interactive'


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

recursive_dirs=''
non_recursive_dirs=''

RecursiveCheckout()
{
    recursive_dirs="$recursive_dirs $*"
}

NonRecursiveCheckout()
{
    non_recursive_dirs="$non_recursive_dirs $*"
}

if test "$srctree" = "development" ; then
  RecursiveCheckout doc
fi

NonRecursiveCheckout src \
    src/connect

RecursiveCheckout src/connect/ext \
    src/connect/services \
    src/connect/test \
    src/corelib \
    src/util \
    src/cgi \
    src/html \
    src/misc \
    src/serial \
    src/app \
    src/algo \
    src/bdb \
    src/build-system

if test "$with_internal" != "no" ; then
    NonRecursiveCheckout src/internal \
        src/internal/ID \
        src/internal/ID/utils

    RecursiveCheckout src/internal/align_model \
        src/internal/log
fi

NonRecursiveCheckout include \
    include/connect

RecursiveCheckout include/connect/ext \
    include/connect/services \
    include/corelib \
    include/common \
    include/util \
    include/cgi \
    include/html \
    include/misc \
    include/serial \
    include/app \
    include/test \
    include/algo \
    include/bdb

if test "$with_internal" != "no" ; then
    NonRecursiveCheckout include/internal \
        include/internal/ID \
        include/internal/ID/utils

    RecursiveCheckout include/internal/align_model \
        include/internal/log
fi

case "$platform" in
  all )
    RecursiveCheckout src/dbapi \
        include/dbapi \
        src/connect/daemons \
        include/connect/daemons \
        compilers \
        src/check
    ;;
  unix )
    NonRecursiveCheckout compilers

    RecursiveCheckout src/dbapi \
        include/dbapi \
        src/connect/daemons \
        include/connect/daemons \
        src/check
    ;;
  msvc )
    NonRecursiveCheckout compilers

    RecursiveCheckout compilers/msvc710_prj \
        compilers/msvc800_prj \
        src/dll \
        src/dbapi \
        include/dbapi \
        src/check
    ;;
  cygwin)
    NonRecursiveCheckout compilers

    RecursiveCheckout compilers/cygwin \
        src/dbapi \
        include/dbapi \
        src/check
    ;;
  mac )
    NonRecursiveCheckout compilers \
        src/dbapi \
        src/dbapi/driver \
        include/dbapi \
        include/dbapi/driver \
        include/dbapi/driver/util

    RecursiveCheckout src/connect/mitsock \
        compilers/mac_prj \
        compilers/xCode
    ;;
esac

case "$platform" in
  all | unix | cygwin | mac )
    RecursiveCheckout scripts
    ;;
  msvc )
    NonRecursiveCheckout scripts

    RecursiveCheckout scripts/check \
        scripts/projects
    ;;
esac

if test "$with_objects" != "no" ; then
    RecursiveCheckout src/objects \
        src/objmgr \
        src/objtools \
        include/objects \
        include/objmgr \
        include/objtools
fi

if test "$with_ctools" != "no" ; then
    RecursiveCheckout src/ctools \
        include/ctools
fi

if test "$export" = "yes" ; then
    for dir in $non_recursive_dirs
    do
        $svn_cmd export -r "$revision" -N $REPOS/$dir $dir || \
            Usage "Cannot checkout $REPOS/$dir"
    done
    for dir in $recursive_dirs
    do
        $svn_cmd export -r "$revision" $REPOS/$dir $dir || \
            Usage "Cannot checkout $REPOS/$dir"
    done
    cmd_for_externals='export'
else
    dirs=''
    for dir in $non_recursive_dirs
    do
        dirs="$dirs $dir/"
    done
    $script_dir/svn_up.pl $dirs $recursive_dirs || \
        Usage "svn_up.pl failed"
    cmd_for_externals='co'
fi

if test "$with_gui" != "no" ; then
    $svn_cmd $cmd_for_externals $GUI_URL/include/gui include/gui || \
        Usage "Cannot checkout $GUI_URL/include/gui"
    $svn_cmd $cmd_for_externals $GUI_URL/src/gui src/gui || \
        Usage "Cannot checkout $GUI_URL/src/gui"
fi

## Generate serialization code from ASN.1 and XML specs
if test "$with_objects" = "yes" ; then
    echo
    echo 'Generating serialization code from ASN.1 and XML specs:'
    scripts/generate_all_objects.sh || \
        Usage "Failed to generate serialization classes"
fi


## Strip some unused stuff
case "$platform" in
  msvc )
    # UNIX-only sources
    rm -f src/connect/ncbi_lbsmd.c src/connect/ncbi_lbsm.c src/connect/ncbi_lbsm_ipc.c \
          include/connect/ext/ncbi_ifconf.h src/connect/ext/ncbi_ifconf.c \
          src/connect/ext/test/test_ncbi_ifconf.c \
          src/cgi/fcgi_run.cpp src/cgi/fcgibuf.cpp || \
        Usage "Cannot remove unused files"
    # scripts and aux. files (mostly "objects/"-related) 
    if test "$with_objects" = "yes" ; then
        rm -f src/objects/add_asn.sh \
              src/objects/nt_sources.sh src/objects/nt_sources.cmd || \
        Usage "Cannot remove unused files"
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
Source tree    : ${srctree}
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
EOF
