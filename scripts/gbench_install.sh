#! /bin/sh
# $Id$
#
# GBENCH installation script.
# Author: Anatoliy Kuznetsov


script=`basename $0`

Usage()
{
    cat <<EOF 1>&2
USAGE: $script [--copy] sourcedir targetdir
SYNOPSIS:
   Creates Genome Workbench installation from the standars toolkit build.
ARGUMENTS:
   --copy     -- When specified uses Unix cp command instead of ln -s
   sourecdir  -- Path name to pre-build NCBI ToolKit
   targetdir  -- Name of the target installation directory.
                 Script creates gbench subdirectory there and installs all
                 necessary files.   
EOF
    test -z "$1" || echo ERROR: $1 1>&2
    exit 1
}

Error()
{
    cat <<EOF 1>&2

ERROR: $1    
EOF
    exit 1
}


ParseInstallFile()
{
    sed_cmd="s/^$1[^a-zA-Z]*=//g"
    while read line; do
        case $line in 
        ${1}*) 
        pl=`echo "$line" | sed $sed_cmd`
        echo $pl
        continue;
        ;;
        esac

        if [ -f tmptmp ]; then
            if [ -z "$line" ]; then
                break;
            fi
            pl=`echo "$line" | sed -e 's/\\\//g'`
            echo $pl
        fi
    done < ${source_dir}/gbench_install/Makefile.gbench_install
}



if [ $# -eq 0 ]; then
    Usage "Wrong number of arguments passed"
fi 

copy_all="no"
if [ $1 = "--copy" ]; then
    copy_all="yes"
    BINCOPY="cp -pf"
    src_dir=$2
    target_dir=$3
else
    BINCOPY="ln -s"
    src_dir=$1
    target_dir=$2
fi


if [ -z "$src_dir" ]; then
    Usage "Source directory not provided."
fi

if [ -z "$target_dir" ]; then
    Usage "Target directory not provided."
fi

if [ ! -d $src_dir ]; then
    Error "Source directory not found: $src_dir"
else
    echo Source: $src_dir    
    echo Target: $target_dir    
fi

if [ ! -d $src_dir/bin ]; then
    Error "bin directory not found: $src_dir/bin"
fi

if [ ! -d $src_dir/lib ]; then
    Error "lib directory not found: $src_dir/lib"
fi

source_dir=`dirname $src_dir`
source_dir=$source_dir/src/gui/gbench/


PLUGIN_COPY_LIST=`ParseInstallFile PLUGINS`
BIN_COPY_LIST=`ParseInstallFile BINS`

# ---------------------------------
# making target directory structure

mkdir -p $target_dir/gbench
mkdir -p $target_dir/gbench/bin
mkdir -p $target_dir/gbench/lib
mkdir -p $target_dir/gbench/etc
mkdir -p $target_dir/gbench/plugins


for x in $BIN_COPY_LIST; do
    echo copying: $x
    mv -f $target_dir/gbench/bin/$x $target_dir/gbench/bin/$x.old  2>/dev/null
    rm -f $target_dir/gbench/bin/$x $target_dir/gbench/bin/$x.old
    cp -p $src_dir/bin/$x $target_dir/gbench/bin/ \
      || Error "Cannot copy file $x"
done

for x in $PLUGIN_COPY_LIST; do
    echo copying plugin: $x
    rm -f $target_dir/gbench/plugins/libgui_$x.so
    $BINCOPY $src_dir/lib/libgui_$x.so $target_dir/gbench/plugins/ \
      || echo "Cannot copy plugin $x"
done

for x in $src_dir/lib/libdbapi*.so; do
    if [ -f $x ]; then
        f=`basename $x`
        echo copying DB interface: $f
        rm -f $target_dir/gbench/lib/$f
        $BINCOPY $x $target_dir/gbench/lib/ \
          ||  Error "Cannot copy $x"
    fi
done


echo preparing scripts 

if [ ! -f $source_dir/gbench_install/run-gbench.sh ]; then
    Error "startup file not found: $source_dir/gbench_install/run-gbench.sh"
fi

cat ${source_dir}/gbench_install/run-gbench.sh \
 | sed "s,@install_dir@,${target_dir}/gbench,g" \
 > ${target_dir}/gbench/bin/run-gbench.sh

chmod 755 ${target_dir}/gbench/bin/run-gbench.sh

if [ -f ${source_dir}/gbench_install/move-gbench.sh ]; then
   cp -p ${source_dir}/gbench_install/move-gbench.sh ${target_dir}/gbench/bin/
fi

cp -p ${source_dir}/gbench.ini ${target_dir}/gbench/etc/  

echo "Configuring plugin cache"
${target_dir}/gbench/bin/gbench_plugin_scan -dir ${target_dir}/gbench/plugins \
 || Error "Plugin scan failed"
