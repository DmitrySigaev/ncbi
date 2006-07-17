#!/bin/sh

# $Id$
# Author:  Andrei Gourianov, NCBI (gouriano@ncbi.nlm.nih.gov)

#-----------------------------------------------------------------------------
#set -xv
#set -x

# defaults
solution="Makefile.flat"
logfile="Flat.configuration_log"
#-----------------------------------------------------------------------------

script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

. ${script_dir}/common.sh


# has one optional argument: error message
Usage()
{
    cat <<EOF 1>&2
USAGE: $script_name BuildDir [-s SrcDir] [-p ProjectList] [-b] 
SYNOPSIS:
 Create flat makefile for a given build tree:
ARGUMENTS:
  BuildDir  -- mandatory. Root directory of the build tree (eg ~/c++/GCC340-Debug)
  -s        -- optional.  Root directory of the source tree (eg ~/c++)
  -p        -- optional.  List of projects: subtree of the source tree, or LST file
  -b        -- optional.  Build project_tree_builder locally
EOF
    test -z "$1"  ||  echo ERROR: $1 1>&2
    exit 1
}


#-----------------------------------------------------------------------------
# analyze script arguments

builddir=""
test $# -lt 1 && Usage "Mandatory arguments missing"
builddir="$1/build"
srcdir="$1/.."
projectlist="src"
buildptb="no"
shift

dest=""
for cmd_arg in "$@"; do
  case "$dest" in
    src  ) srcdir="$cmd_arg"; dest=""; continue ;;
    prj  ) projectlist="$cmd_arg"; dest=""; continue ;;
    * )  dest="" ;;
  esac
  case "$cmd_arg" in
    -s )  dest="src" ;;
    -p )  dest="prj" ;;
    -b )  buildptb="yes"; dest="" ;;
    *  )  Usage "Invalid command line argument:  $cmd_arg"
  esac
done

test -d "$builddir"  || Usage "$builddir is not a directory"
test -d "$srcdir"    || Usage "$srcdir is not a directory"
if test ! -f "$srcdir/$projectlist"; then
  test -d "$srcdir/$projectlist" || Usage "$srcdir/$projectlist not found"
fi

#-----------------------------------------------------------------------------
# more checks

cd $builddir
dll=""
test -f "../status/DLL.enabled" && dll="-dll"
ptbini="$srcdir/compilers/msvc710_prj/project_tree_builder.ini"
test -f "$ptbini" || Usage "$ptbini not found"

#-----------------------------------------------------------------------------
# build project_tree_builder

cd $builddir
if test "$buildptb" = "yes"; then
  ptbdep="corelib util serial serial/datatool app/project_tree_builder"
  for dep in $ptbdep; do
    test -d "$dep" || Usage "$builddir/$dep not found"
    test -f "$dep/Makefile" || Usage "$builddir/$dep/Makefile not found"
  done

  echo "**********************************************************************"
  echo "Building project_tree_builder"
  echo "**********************************************************************"
  for dep in $ptbdep; do
    COMMON_Exec cd $builddir
    COMMON_Exec cd $dep
    COMMON_Exec make
  done
  cd $builddir
  ptb="./app/project_tree_builder/project_tree_builder"
  test -f "$ptb" || Usage "$builddir/$ptb not found"
else
  ptb="$NCBI/c++/Release/bin/project_tree_builder"
  test -f "$ptb" || Usage "$ptb not found"
fi

cd $builddir
echo "**********************************************************************"
echo "Running project_tree_builder. Please wait."
echo "**********************************************************************"
echo $ptb $dll -conffile $ptbini -logfile $logfile $srcdir $projectlist $solution
COMMON_Exec $ptb $dll -conffile $ptbini -logfile $logfile $srcdir $projectlist $solution
echo "Done"

