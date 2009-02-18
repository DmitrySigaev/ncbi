#!/bin/sh

# $Id$
# Author:  Andrei Gourianov, NCBI (gouriano@ncbi.nlm.nih.gov)

#-----------------------------------------------------------------------------
#set -xv
#set -x

# defaults
solution="Makefile.flat"
logfile="Flat.configuration_log"
ptbname="project_tree_builder"

# default path to project_tree_builder
defptbpath="$NCBI/c++.metastable/Release/bin/"
# required version of PTB
# NOTE: relptbver, when defined, overrides this setting !
ptbreqver=173

# release path to project_tree_builder
relptbpath="/net/snowman/vol/export2/win-coremake/App/Ncbi/cppcore/ptb/"
# NOTE: redefine relptbver in stable components release, eg "1.7.3" !
relptbver=""

# dependencies
ptbdep="corelib util util/regexp util/xregexp build-system/project_tree_builder"
#-----------------------------------------------------------------------------

initial_dir=`pwd`
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

. ${script_dir}/../common.sh


# has one optional argument: error message
Usage()
{
    cat <<EOF 1>&2
USAGE: $script_name BuildDir [-s SrcDir] [-p ProjectList] [-b] 
SYNOPSIS:
 Create flat makefile for a given build tree.
ARGUMENTS:
  BuildDir  -- mandatory. Root dir of the build tree (eg ~/c++/GCC340-Debug)
  -s        -- optional.  Root dir of the source tree (eg ~/c++)
  -p        -- optional.  List of projects: subtree of the source tree, or LST file
  -b        -- optional.  Build project_tree_builder locally
EOF
    test -z "$1"  ||  echo ERROR: $1 1>&2
    exit 1
}

DetectPlatform()
{
    osname="`uname`"
    if [ "$osname" = "Linux" ]; then
        case "`arch`" in
            *86 )
                PLATFORM="Linux32"
                ;;
            *86_64 )
                PLATFORM="Linux64"
                ;;
        esac
    elif [ "$osname" = "SunOS" ]; then
        case "`arch`" in
            sun* )
                PLATFORM="SunOSSparc"
                ;;
            i86pc )
                PLATFORM="SunOSx86"
                ;;
        esac
    elif [ "$osname" = "IRIX64" ]; then
        PLATFORM="IRIX64"
    elif [ "$osname" = "Darwin" ]; then
        case "`arch`" in
            ppc )
                PLATFORM="PowerMAC"
                ;;
	    i386 )
                PLATFORM="IntelMAC"
                ;;
        esac
    elif [ "$osname" = "FreeBSD" ]; then
        case "`uname -m`" in
            i386 )
                PLATFORM="FreeBSD32"
                ;;
        esac
    elif `echo "$osname" | grep -q -s CYGWIN_NT` ; then
        case "`uname -m`" in
            *86 )
                PLATFORM="Win32"
                ;;
                # Unverified!
            *64 )
                PLATFORM="Win64"
                ;;
        esac
    fi

    if [ -z "$PLATFORM" ]; then
        echo "Platform not defined for $osname -- please fix me"
        uname -a
        exit 1
    fi
}

#-----------------------------------------------------------------------------
# analyze script arguments


test $# -lt 1 && Usage "Mandatory argument is missing"
COMMON_Exec cd $initial_dir
COMMON_Exec cd $1
a1=`pwd`
builddir="$a1/build"
srcdir="$a1/.."
projectlist="src"
buildptb="no"
shift

dest=""
for cmd_arg in "$@"; do
  case "$dest" in
    src  )  dest="";  srcdir="$cmd_arg"     ;  continue ;;
    prj  )  dest="";  projectlist="$cmd_arg";  continue ;;
    *    )  dest=""                                     ;;
  esac
  case "$cmd_arg" in
    -s )  dest="src" ;;
    -p )  dest="prj" ;;
    -b )  dest="";    buildptb="yes" ;;
    *  )  Usage "Invalid command line argument:  $cmd_arg"
  esac
done

test -d "$builddir"  || Usage "$builddir is not a directory"
test -d "$srcdir"    || Usage "$srcdir is not a directory"
case "$projectlist" in
  /* ) abs_projectlist=$projectlist ;;
  *  ) abs_projectlist=$srcdir/$projectlist ;;
esac
if test ! -f "$abs_projectlist"; then
  test -d "$abs_projectlist" || Usage "$abs_projectlist not found"
fi


#-----------------------------------------------------------------------------
# more checks
extptb="$defptbpath$ptbname"
ptb="$PREBUILT_PTB_EXE"
if test -x "$ptb"; then
  echo "Using prebuilt project tree builder at $ptb"
else
  ptb="$extptb"
  if test $buildptb = "no"; then
    if test -n "$relptbver"; then
      DetectPlatform
      ptb="$relptbpath$PLATFORM/$relptbver/$ptbname"
      if test -x "$ptb"; then
        echo "Using prebuilt project tree builder at $ptb"
      else
        echo "Prebuilt $ptbname is not found at"
        echo $ptb
        echo "Will build PTB locally"
        buildptb="yes"
      fi
    else
      if test -x "$ptb"; then
        echo "Testing $ptb"
        $ptb -version 2>&1
        ptbver=`$ptb -version 2>&1 | grep $ptbname | sed -e 's/[a-zA-Z._: ]//g'`
        if test $ptbver -lt $ptbreqver; then
          echo "Prebuilt $ptbname at"
          echo $extptb
          echo "is too old. Will build PTB locally"
          buildptb="yes"
        fi
      else
        echo "Prebuilt $ptbname is not found at"
        echo $extptb
        echo "Will build PTB locally"
        buildptb="yes"
      fi
    fi
  fi
fi

echo $ptb
echo $buildptb
exit 1
COMMON_Exec cd $builddir
dll=""
test -f "../status/DLL.enabled" && dll="-dll"
ptbini="$srcdir/src/build-system/$ptbname.ini"
test -f "$ptbini" || Usage "$ptbini not found"


#-----------------------------------------------------------------------------
# build project_tree_builder

COMMON_Exec cd $builddir
if test "$buildptb" = "yes"; then
  for dep in $ptbdep; do
    if test ! -d "$dep"; then
      echo "WARNING: $builddir/$dep not found"
      buildptb="no"
      break;
    fi
    if test ! -f "$dep/Makefile"; then
      echo "WARNING: $builddir/$dep/Makefile not found"
      buildptb="no"
      break;
    fi
  done
fi

if test "$buildptb" = "yes"; then
  echo "**********************************************************************"
  echo "Building $ptbname"
  echo "**********************************************************************"
  for dep in $ptbdep; do
    COMMON_Exec cd $builddir
    COMMON_Exec cd $dep
    COMMON_Exec make
  done
  COMMON_Exec cd $builddir
  ptb="./build-system/project_tree_builder/$ptbname"
  test -x "$ptb" || Usage "$builddir/$ptb not found"
fi

test -x "$ptb" || Usage "$ptb not found"

#-----------------------------------------------------------------------------
# run project_tree_builder

COMMON_Exec cd $builddir
echo "**********************************************************************"
echo "Running $ptbname. Please wait."
echo "**********************************************************************"
echo $ptb $dll -conffile $ptbini -logfile $logfile $srcdir $projectlist $solution
COMMON_Exec $ptb $dll -conffile $ptbini -logfile $logfile $srcdir $projectlist $solution
echo "Done"
