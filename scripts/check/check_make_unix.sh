#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Compile a check script and copy necessary files to run tests in the 
# UNIX build tree.
#
# Usage:
#    check_make_unix.sh <test_list> <build_dir> <target_dir> <check_script>
#
#    test_list       - a list of tests (it build with "make check_r")
#                      (default: "<build_dir>/check.sh.list")
#    build_dir       - path to UNIX build tree like".../build/..."
#                      (default: will try determine path from current work
#                      directory -- root of build tree ) 
#    target_dir      - path where the check script and logs will be created
#                      (default: current dir) 
#    check_script    - name of the check script (without path).
#                      (default: "check.sh" / "<target_dir>/check.sh")
#
#    If any parameter is skipped that will be used default value for it.
#
# Note:
#    Work with UNIX build tree only (any configuration).
#
###########################################################################

# Parameters

res_out="check.sh"
res_list="$res_out.list"

# Fields delimiters in the list (this symbols used directly in the "sed" command)
x_delim=" ____ "
x_delim_internal="~"
x_tmp="/var/tmp"

x_list=$1
x_build_dir=$2
x_target_dir=$3
x_out=$4

# Check for build dir
if [ ! -z "$x_build_dir" ]; then
   if [ ! -d "$x_build_dir" ]; then
      echo "Build directory \"$x_build_dir\" don't exist."
      exit 1 
   fi
   x_build_dir=`(cd "$x_build_dir"; pwd | sed -e 's/\/$//g')`
else
   # Get build dir name from the current path
   x_build_dir=`pwd | sed -e 's%/build.*$%%'`
   if [ -d "$x_build_dir/build" ]; then
      x_build_dir="$x_build_dir/build"
   fi
fi

# Bin dir
x_bin_dir=`(cd "$x_build_dir/../bin"; pwd | sed -e 's/\/$//g')`

# Check for target dir
if [ ! -z "$x_target_dir" ]; then
   if [ ! -d "$x_target_dir" ]; then
      echo "Target directory \"$x_target_dir\" don't exist."
      exit 1 
   fi
    x_target_dir=`(cd "$x_target_dir"; pwd | sed -e 's/\/$//g')`
else
   x_target_dir=`pwd`
fi

x_conf_dir=`dirname "$x_build_dir"`
x_root_dir=`dirname "$x_conf_dir"`

# Check for a imported project or intree project
res=`pwd | grep "/internal/c++/src/"`
if [ ! -z "$res" ] ; then
   x_import_prj="yes"
   x_compile_dir="`pwd | sed -e 's%/internal/c++/src.*$%%g'`/internal/c++/src"
else
   x_import_prj="no"
   x_compile_dir="$x_build_dir"
fi

if [ -z "$x_list" ]; then
   x_list="$x_target_dir/$res_list"
fi

if [ -z "$x_out" ]; then
   x_out="$x_target_dir/$res_out"
fi

x_script_name=`echo "$x_out" | sed -e 's%^.*/%%'`

# Check for a list file
if [ ! -f "$x_list" ]; then
   echo "Check list file \"$x_list\" not found."
   exit 1 
fi


#echo ----------------------------------------------------------------------
#echo "Imported project  :" $x_import_prj
#echo "C++ root dir      :" $x_root_dir
#echo "Configuration dir :" $x_conf_dir
#echo "Build dir         :" $x_build_dir
#echo "Compile dir       :" $x_compile_dir
#echo "Target dir        :" $x_target_dir
#echo "Check script      :" $x_out
#echo ----------------------------------------------------------------------

#//////////////////////////////////////////////////////////////////////////

cat > $x_out <<EOF
#! /bin/sh

res_journal="$x_out.journal"
res_log="$x_out.log"
res_list="$x_list"
res_concat="$x_out.out"


##  Printout USAGE info and exit

Usage() {
   cat <<EOF_usage

USAGE:  $x_script_name {run | clean | concat | concat_err}

 run         Run the tests. Create output file ("*.test_out") for each test, 
             plus journal and log files. 
 clean       Remove all files created during the last "run" and this script 
             itself.
 concat      Concatenate all files created during the last "run" into one big 
             file "\$res_log".
 concat_err  Like previous. But into the file "\$res_concat" 
             will be added outputs of failed tests only.

ERROR:  \$1
EOF_usage

    exit 1
}

if [ \$# -ne 1 ]; then
   Usage "Invalid number of arguments."
fi


###  What to do (cmd-line arg)

method="\$1"


### Action

case "\$method" in
#----------------------------------------------------------
   run )
      ;;
#----------------------------------------------------------
   clean )
      x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
      for x_file in \$x_files; do
         x_file=\`echo "\$x_file" | sed -e 's/%gj_s4%/ /g'\`
         rm -f \$x_file > /dev/null
      done
      rm -f \$res_journal \$res_log \$res_list \$res_concat > /dev/null
      rm -f $x_out > /dev/null
      exit 0
      ;;
#----------------------------------------------------------
   concat )
      rm -f "\$res_concat"
      ( 
      cat \$res_log
      x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
      for x_file in \$x_files; do
         x_file=\`echo "\$x_file" | sed -e 's/%gj_s4%/ /g'\`
         echo 
         echo 
         cat \$x_file
      done
      ) >> \$res_concat
      exit 0
      ;;
#----------------------------------------------------------
   concat_err )
      rm -f "\$res_concat"
      ( 
      cat \$res_log | grep 'ERR \['
      x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
      for x_file in \$x_files; do
         x_file=\`echo "\$x_file" | sed -e 's/%gj_s4%/ /g'\`
         x_code=\`cat \$x_file | grep -c '@@@ EXIT CODE:'\`
         test \$x_code -ne 0 || continue
         x_good=\`cat \$x_file | grep -c '@@@ EXIT CODE: 0'\`
         if [ \$x_good -ne 1 ]; then
            echo 
            echo 
            cat \$x_file
         fi
      done
      ) >> \$res_concat
      exit 0
      ;;
#----------------------------------------------------------
   * )
      Usage "Invalid method name."
      ;;
esac

# Export some global vars
top_srcdir="$x_root_dir"
export top_srcdir

# Add current, build and scripts directories to PATH
PATH=".:${x_build_dir}:${x_root_dir}/scripts:\${PATH}"
export PATH

EOF

if [ -n "$x_conf_dir"  -a  -d "$x_conf_dir/lib" ];  then
   cat >> $x_out <<EOF
# Adjust PATH and LD_LIBRARY_PATH for running tests
if [ -n "\$LD_LIBRARY_PATH" ]; then
   LD_LIBRARY_PATH="${x_conf_dir}/lib:\${LD_LIBRARY_PATH}"
else
   LD_LIBRARY_PATH="$x_conf_dir/lib"
fi
export LD_LIBRARY_PATH
EOF
else
   echo "WARNING:  Cannot find path to the library dir."
fi

   cat >> $x_out <<EOF

# Run
count_ok=0
count_err=0
count_absent=0

rm -f "\$res_journal"
rm -f "\$res_log"

ulimit -c 1000000

##  Run one test

RunTest() {
echo \$top_srcdir

   # Parameters
   x_work_dir_tail="\$1"
   x_work_dir="$x_compile_dir/\$x_work_dir_tail"
   x_test="\$2"
   x_app="\$3"
   x_run="\$4"
   x_test_out="\$x_work_dir/\$x_test.\$5"
   x_timeout="\$6"

   # Check existence of the test's application directory
   if [ -d "\$x_work_dir" ]; then
      # Write header to output file 
      echo "\$x_test_out" >> \$res_journal
      (
        echo "======================================================================"
        echo "\$x_run"
        echo "======================================================================"
        echo 
      ) > \$x_test_out 2>&1

      # Remove old core file if it exist (for clarity of the test)
      rm -f "\$x_work_dir/core" > /dev/null

      # Goto the test's directory 
      cd "\$x_work_dir"
       x_cmd="[\$x_work_dir_tail] \$x_run"

      # And run test if it exist
      if [ -f "\$x_app" ]; then
         # Fix empty parameters (replace "" to \"\", '' to \'\')
         x_run_fix=\`echo "\$x_run" | sed -e 's/""/\\\\\\\\\\"\\\\\\\\\\"/g' -e "s/''/\\\\\\\\\\'\\\\\\\\\\'/g"\`
         # Run check
         check_exec="$x_root_dir/scripts/check/check_exec.sh"
         \$check_exec \$x_timeout time -p \`eval echo \$x_run_fix\` >$x_tmp/\$\$.out 2>&1
         result=\$?

         sed -e '/ ["][$][@]["].*\$/ {
            s/^.*: //
            s/ ["][$][@]["].*$//
            }' $x_tmp/\$\$.out >> \$x_test_out

         # Get application execution time
         if [ \$result -eq 0 ]; then
            exec_time=\`tail -3 $x_tmp/\$\$.out | tr '\n' '?'\`
            exec_time=\`echo \$exec_time | sed -e 's/?$//' -e 's/?/, /g' -e 's/[ ] */ /g'\`
         fi
         rm -f $x_tmp/\$\$.out

         # Write result of the test into the his output file
         echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> \$x_test_out
         echo "@@@ EXIT CODE: \$result" >> \$x_test_out

         if [ -f "\$x_work_dir/core" ]; then
            echo "@@@ CORE DUMPED" >> \$x_test_out
		    if [ -d "$x_bin_dir" ]; then
               mv "\$x_work_dir/core" "$x_bin_dir/\$x_test.core"
            else
               rm -f "\$x_work_dir/core"
		    fi
         fi

         # And write result also on the screen and into the log
         if [ \$result -eq 0 ]; then
            echo "OK  --  \$x_cmd     (\$exec_time)"
            echo "OK  --  \$x_cmd     (\$exec_time)" >> \$res_log
            count_ok=\`expr \$count_ok + 1\`
         else
            echo "ERR --  \$x_cmd"
            echo "ERR [\$result] --  \$x_cmd" >> \$res_log
            count_err=\`expr \$count_err + 1\`
         fi
      else
         echo "ABS --  \$x_cmd"
         echo "ABS --  \$x_cmd" >> \$res_log
         count_absent=\`expr \$count_absent + 1\`
      fi
  else
      # Test application is absent
      echo "ABS -- \$x_work_dir - \$x_test"
      echo "ABS -- \$x_work_dir - \$x_test" >> \$res_log
      count_absent=\`expr \$count_absent + 1\`
  fi
}

EOF

#//////////////////////////////////////////////////////////////////////////


# Read list with tests
x_tests=`cat "$x_list" | sed -e 's/ /%gj_s4%/g'`
x_test_prev=""

# For all tests
for x_row in $x_tests; do
   # Get one row from list
   x_row=`echo "$x_row" | sed -e 's/%gj_s4%/ /g' -e 's/^ *//' -e 's/ ____ /~/g'`

   # Split it to parts
   x_src_dir="$x_root_dir/src/`echo \"$x_row\" | sed -e 's/~.*$//'`"
   x_test=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_app=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_cmd=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_files=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_timeout=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//'  -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_requires=" `echo \"$x_row\" | sed -e 's/.*~//'` "

   # Check application requirements
   for x_req in $x_requires; do
      test -f "$x_conf_dir/status/$x_req.enabled"  ||  continue 2
   done

   # Application base build directory
   x_work_dir_tail="`echo \"$x_row\" | sed -e 's/~.*$//'`"
   x_work_dir="$x_compile_dir/$x_work_dir_tail"

   # Copy specified files to the build directory

   if [ "$x_import_prj" = "no" ]; then
      if [ ! -z "$x_files" ]; then
         for i in $x_files ; do
            x_copy="$x_src_dir/$i"
            if [ -f "$x_copy"  -o  -d "$x_copy" ]; then
               cp -prf "$x_copy" "$x_work_dir"
            else
               echo "Warning:  The copied object \"$x_copy\" should be a file or directory!"
               continue 1
            fi
         done
      fi
   fi

   # Generate extension for tests output file
   if [ "$x_test" != "$x_test_prev" ]; then 
      x_cnt=1
      x_test_out="test_out"
   else
      x_cnt=`expr $x_cnt + 1`
      x_test_out="test_out$x_cnt"
   fi
   x_test_prev="$x_test"

#//////////////////////////////////////////////////////////////////////////

   # Write test commands for current test into a shell script file
   cat >> $x_out <<EOF
######################################################################
RunTest "$x_work_dir_tail" \\
        "$x_test" \\
        "$x_app" \\
        "$x_cmd" \\
        "$x_test_out" \\
        "$x_timeout"
EOF

#//////////////////////////////////////////////////////////////////////////

done # for x_row in x_tests


# Write ending code into the script 
cat >> $x_out <<EOF


# Write result of the tests execution
echo
echo "Succeeded : \$count_ok"
echo "Failed    : \$count_err"
echo "Absent    : \$count_absent"
echo

if [ \$count_err -eq 0 ]; then
   echo
   echo "******** ALL TESTS COMPLETED SUCCESSFULLY ********"
   echo
fi

exit \$count_err
EOF

# Set execute mode to script
chmod a+x "$x_out"

exit 0
