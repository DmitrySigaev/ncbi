#! /bin/sh

# $Id$
# Author:  Denis Vakatov, NCBI 
#
###########################################################################
#
#  Auxilary script -- to be called by "./Makefile.check"
#
#  "file:[full:]/abs_fname  file:[full:]rel_fname
#   mail:[full:]addr1,addr2,..  post:[full:]url  watch:addr1,addr2,.."
#
###########################################################################


script_name=`basename $0`
script_args="$*"

short_res="check.sh.log"
full_res="check.sh.out"


#####  ERRORS
err_log="/tmp/check.sh.err.$$"
exec 2>$err_log
trap "rm -f $err_log" 1 2 15

Error()
{
   cat <<EOF 1>&2

-----------------------------------------------------------------------

$script_name $script_args

ERROR:  $1
EOF

   cat $err_log

   kill $$
   exit 1
}


####  ARGS

test $# -gt 3 ||  Error "Wrong number of args:  $#"
test -d "$3"  ||  Error "Is not a directory:  $2"

signature="$1"
make="$2"
builddir="$3"
action="$4"

shift
shift
shift
shift

cd "$builddir"  ||  Error "Cannot CHDIR to directory:  $builddir"

case "$action" in
 all )
   # continue
   ;;
 clean | purge )
   test -x ./check.sh  &&  ./check.sh clean
   exit 0
   ;;
 * )
   Error "Invalid action: $action"
   ;;
esac


####  RUN CHECKS

"$make" check_r RUN_CHECK=Y


####  POST RESULTS

# Parse the destination location list
for dest in "$@" ; do
   type=`echo "$dest" | sed 's%\([^:][^:]*\):.*%\1%'`
   loc=`echo "$dest" | sed 's%.*:\([^:][^:]*\)$%\1%'`

   full=""
   echo "$dest" | grep ':full:' >/dev/null  &&  full="yes"

   case "$type" in
    file )
      echo "$loc" | grep '^/' >/dev/null ||  loc="$builddir/$loc"
      loc=`echo "$loc" | sed 's%{}%'${signature}'%g'`
      if test -n "$full" ; then
         file_list_full="$file_list_full $loc"
      else
         file_list="$file_list $loc"
      fi
      ;;
    mail )
      if test -n "$full" ; then
         mail_list_full="$mail_list_full $loc"
      else
         mail_list="$mail_list $loc"
      fi
      ;;
    watch )
      loc=`echo "$loc" | sed 's/,/ /g'`
      watch_list="$watch_list $loc"
      ;;
    * )
      err_list="$err_list BAD_TYPE:\"$dest\""
      ;;
   esac
done

# Compose the "full" results archive, if necessary
echo "$*" | grep ':full:' >/dev/null  &&  ./check.sh concat

# Post results to the specified locations
if test -n "$file_list_full" ; then
   for loc in $file_list_full ; do
      cp -p $full_res $loc  ||  \
         err_list="$err_list COPY_ERR:\"$loc\""
   done
fi

if test -n "$file_list" ; then
   for loc in $file_list ; do
      cp -p $short_res $loc  ||  \
         err_list="$err_list COPY_ERR:\"$loc\""
   done
fi

subject="[C++ CHECK RESULTS]  ${signature}"

if test -n "$mail_list_full" ; then
   for loc in $mail_list_full ; do
      mailto=`echo "$loc" | sed 's/,/ /g'`
      cat $full_res | mailx -s "$subject" $mailto  || \
         err_list="$err_list MAIL_ERR:\"$loc\""
   done
fi

if test -n "$mail_list" ; then
   for loc in $mail_list ; do
      mailto=`echo "$loc" | sed 's/,/ /g'`
      cat $short_res | mailx -s "$subject" $mailto  ||  \
         err_list="$err_list MAIL_ERR:\"$loc\""
   done
fi

# Post errors to watchers
if test -n "$err_list" ; then
   ( echo "$err_list" ; echo ; echo "====================" ; cat $err_log ) | \
    mailx -s "[C++ CHECK ERR]  ${signature}" $watch_list
fi


# Cleanup
rm -f $err_log
exit 0
