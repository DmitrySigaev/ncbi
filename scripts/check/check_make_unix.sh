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
#    top_srcdir      - path to the root src directory
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

# Fields delimiters in the list
# (this symbols used directly in the "sed" command)
x_delim=" ____ "
x_delim_internal="~"
x_tmp="/var/tmp"

x_list=$1
x_build_dir=$2
x_top_srcdir=$3
x_target_dir=$4
x_out=$5


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

x_conf_dir=`dirname "$x_build_dir"`
x_bin_dir=`(cd "$x_build_dir/../bin"; pwd | sed -e 's/\/$//g')`

# Check for top_srcdir
if [ ! -z "$x_top_srcdir" ]; then
   if [ ! -d "$x_top_srcdir" ]; then
      echo "Top source directory \"$x_top_srcdir\" don't exist."
      exit 1 
   fi
   x_root_dir=`(cd "$x_top_srcdir"; pwd | sed -e 's/\/$//g')`
else
   # Get top src dir name from the script directory
   x_check_scripts_dir=`dirname "$0"`
   x_scripts_dir=`dirname "$x_check_scripts_dir"`
   x_root_dir=`dirname "$x_scripts_dir"`
fi

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

# Features detection
x_features=""
for f in $x_conf_dir/status/*.enabled; do
   f=`echo $f | sed 's|^.*/status/\(.*\).enabled$|\1|g'`
   x_features="$x_features$f "
done


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

root_dir="$x_root_dir"
build_dir="$x_build_dir"
conf_dir="$x_conf_dir"
compile_dir="$x_compile_dir"
bin_dir="$x_bin_dir"
script="$x_out"

res_journal="\$script.journal"
res_log="\$script.log"
res_list="$x_list"
res_concat="\$script.out"
res_concat_err="\$script.out_err"


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
 concat_err  Like previous. But into the file "\$res_concat_err" 
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
      rm -f \$res_journal \$res_log \$res_list \$res_concat \$res_concat_err > /dev/null
      rm -f \$script > /dev/null
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
      rm -f "\$res_concat_err"
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
      ) >> \$res_concat_err
      exit 0
      ;;
#----------------------------------------------------------
   * )
      Usage "Invalid method name."
      ;;
esac


# Include configuration file
. \${build_dir}/check.cfg
if [ -z "\$NCBI_CHECK_TOOLS" ]; then
   NCBI_CHECK_TOOLS="regular"
fi

# Valgrind configuration
VALGRIND_SUP="\${root_dir}/scripts/check/valgrind.supp"
VALGRIND_CMD="--tool=memcheck --suppressions=\$VALGRIND_SUP"

# Export some global vars
top_srcdir="\$root_dir"
export top_srcdir
FEATURES="$x_features"
export FEATURES

# Add current, build and scripts directories to PATH
PATH=".:\${build_dir}:\${root_dir}/scripts:\${PATH}"
export PATH

# Export bin and lib pathes
CFG_BIN="\${conf_dir}/bin"
CFG_LIB="\${conf_dir}/lib"
export CFG_BIN CFG_LIB

# Define time-guard script to run tests from other scripts
check_exec="\$root_dir/scripts/check/check_exec.sh"
CHECK_EXEC="\${root_dir}/scripts/check/check_exec_test.sh"
CHECK_EXEC_STDIN="\$CHECK_EXEC -stdin"
export CHECK_EXEC
export CHECK_EXEC_STDIN

# Avoid possible hangs on Mac OS X.
DYLD_BIND_AT_LAUNCH=1
export DYLD_BIND_AT_LAUNCH

EOF

if [ -n "$x_conf_dir"  -a  -d "$x_conf_dir/lib" ];  then
   cat >> $x_out <<EOF
# Add a library path for running tests
. \$root_dir/scripts/common.sh
COMMON_AddRunpath "\$conf_dir/lib"
EOF
else
   echo "WARNING:  Cannot find path to the library dir."
fi

cat >> $x_out <<EOF

# Run
count_ok=0
count_err=0
count_absent=0
count_total=0

rm -f "\$res_journal"
rm -f "\$res_log"

ulimit -c 1000000

##  Run one test

RunTest() {

   # Parameters
   x_work_dir_tail="\$1"
   x_work_dir="\$compile_dir/\$x_work_dir_tail"
   x_test="\$2"
   x_app="\$3"
   x_run="\$4"
   x_ext="\$5"
   x_timeout="\$6"

   count_total=\`expr \$count_total + 1\`
   x_log="$x_tmp/\$\$.out\$count_total"

   # Check existence of the test's application directory
   if [ -d "\$x_work_dir" ]; then

      # Goto the test's directory 
      cd "\$x_work_dir"
      x_cmd="[\$x_work_dir_tail] \$x_run"

      # Run test if it exist
      if [ -f "\$x_app" ]; then

         CHECK_TIMEOUT="\$x_timeout"
         export CHECK_TIMEOUT
         _RLD_ARGS="-log \$x_log"
         export _RLD_ARGS

         # Fix empty parameters (replace "" to \"\", '' to \'\')
         x_run_fix=\`echo "\$x_run" | sed -e 's/""/\\\\\\\\\\"\\\\\\\\\\"/g' -e "s/''/\\\\\\\\\\'\\\\\\\\\\'/g"\`

         # Run test under all specified check tools   
         for tool in \$NCBI_CHECK_TOOLS; do

            tool_lo=\`echo \$tool | tr '[:upper:]' '[:lower:]'\`
            tool_up=\`echo \$tool | tr '[:lower:]' '[:upper:]'\`
            NCBI_CHECK_TOOL=\`eval echo "\$"NCBI_CHECK_\${tool_up}""\`
      
            if [ \$tool_lo = "regular" ] ; then
               x_cmd="[\$x_work_dir_tail] \$x_run"
               x_test_out="\$x_work_dir/\$x_test.\$x_ext"
            else
               x_cmd="[\$x_work_dir_tail] \$tool_up \$x_run"
               x_test_out="\$x_work_dir/\$x_test.\$x_ext.\$tool_lo"
            fi
            case "\$tool_lo" in
               regular  ) ;;
               valgrind ) NCBI_CHECK_TOOL="\$NCBI_CHECK_TOOL \$VALGRIND_CMD" ;;
                      * ) NCBI_CHECK_TOOL="?" ;;
            esac
         
            if [ ".\$NCBI_CHECK_TOOL" = ".?" ] ; then
               result=255;
               exec_time="Unknown check tool \$tool_up"
            else
               export NCBI_CHECK_TOOL
         
               echo \$x_run | grep '.sh' > /dev/null 2>&1 
               if [ \$? -eq 0 ] ;  then
                  # Run script without any check tools.
                  # It will be applied inside script using $CHECK_EXEC.
                  xx_run="\$x_run_fix"
               else
                  # Run under check tool
                  xx_run="\$NCBI_CHECK_TOOL \$x_run_fix"
               fi

               # Write header to output file 
               echo "\$x_test_out" >> \$res_journal
               (
                 echo "======================================================================"
                 echo "\$x_run"
                 echo "======================================================================"
                 echo 
               ) > \$x_test_out 2>&1

               # Remove old core file if it exist (for clarity of the test)
               corefile="\$x_work_dir/core"
               rm -f "\$corefile"  > /dev/null 2>&1

               # Run check
               start_time="\`date\`"
               \$check_exec time -p \`eval echo \$xx_run\` >\$x_log 2>&1
               result=\$?
               stop_time="\`date\`"

               sed -e '/ ["][$][@]["].*\$/ {
                  s/^.*: //
                  s/ ["][$][@]["].*$//
               }' \$x_log >> \$x_test_out

               # Get application execution time
               exec_time=\`\$build_dir/sysdep.sh tl 5 \$x_log | tr '\n\r' '??'\`
               exec_time=\`echo \$exec_time |  \\
                          sed -e 's/??/?/g'    \\
                              -e 's/?$//'      \\
                              -e 's/?/, /g'    \\
                              -e 's/[ ] */ /g' \\
                              -e 's/^.*\(real [0-9][0-9]*[.][0-9][0-9]*\)/\1/' \\
                              -e 's/\(sys [0-9][0-9]*[.][0-9][0-9]*\).*/\1/' \\
                              -e 's/.*\(Maximum execution .* is exceeded\).*$/\1/'\`

               # Analize check tool output
               case "\$tool_lo" in
                  valgrind ) summary_all=\`grep -c 'ERROR SUMMARY:' \$x_test_out\`
                             summary_ok=\`grep -c 'ERROR SUMMARY: 0 ' \$x_test_out\`
                             # The number of given lines can be zero.
                             # In some cases we can lost valgrind's summary.
                             if [ \$summary_all -ne \$summary_ok ]; then
                                result=254
                             fi
                             ;;
               esac

               # CVS tree checkout date
               cvs_checkout=''
               if [ -f "\$root_dir/checkout.date" ] ; then
                  cvs_checkout=\`cat \$root_dir/checkout.date\`
               fi

               # Write result of the test into the his output file
               echo "Start time  : \$start_time"   >> \$x_test_out
               echo "Stop time   : \$stop_time"    >> \$x_test_out
               echo "CVS checkout: \$cvs_checkout" >> \$x_test_out
               echo >> \$x_test_out
               echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> \$x_test_out
               echo "@@@ EXIT CODE: \$result" >> \$x_test_out

               if [ -f "\$corefile" ]; then
                  echo "@@@ CORE DUMPED" >> \$x_test_out
                  if [ -d "\$bin_dir" -a -f "\$bin_dir/\$x_test" ]; then
                     mv "\$corefile" "\$bin_dir/\$x_test.core"
                  else
                     rm -f "\$corefile"
                  fi
               fi
            fi

            # Write result also on the screen and into the log
            if [ \$result -eq 0 ]; then
               echo "OK  --  \$x_cmd     (\$exec_time)"
               echo "OK  --  \$x_cmd     (\$exec_time)" >> \$res_log
               count_ok=\`expr \$count_ok + 1\`
            else
               echo "ERR [\$result] --  \$x_cmd     (\$exec_time)"
               echo "ERR [\$result] --  \$x_cmd     (\$exec_time)" >> \$res_log
               count_err=\`expr \$count_err + 1\`
            fi
         done
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
