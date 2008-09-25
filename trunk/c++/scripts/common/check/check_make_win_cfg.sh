#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Compile a check script and copy necessary files to run tests in the 
# MS VisualC++ build tree.
#
# Usage:
#    check_make_win_cfg.sh <action> <solution> <static|dll> <cfg> [build_dir]
#
#    <action>      - { init | create } 
#                      init   - initialize master script directory;
#                      create - create check script.
#    <solution>    - solution file name without .sln extention
#                    (relative path from build directory).
#    <static|dll>  - type of used libraries (static, dll).
#    <cfg>         - configuration name
#                    (Debug, DebugDLL, DebugMT, Release, ReleaseDLL, ReleaseMT).
#    [build_dir]   - path to MSVC build tree like ".../msvc_prj"
#                    (default: will try determine path from current work
#                    directory -- root of build tree) 
#
###########################################################################


# Field delimiters in the list (this symbols used directly in the "sed" command)
x_delim=" ____ "
x_tmp="/tmp"
x_date_format="%m/%d/%Y %H:%M:%S"

# Arguments
x_action=$1
x_solution=$2
x_libdll=$3
x_cfg=$4
x_build_dir=$5

if test ! -z "$x_build_dir"; then
   if test ! -d "$x_build_dir"; then
      echo "Build directory \"$x_build_dir\" don't exist."
      exit 1 
   fi
   # Expand path and remove trailing slash
   x_build_dir=`(cd "$x_build_dir"; pwd | sed -e 's/\/$//g')`
else
   # Get build dir name from current work directory
   x_build_dir=`pwd`
fi
x_root_dir=`echo "$x_build_dir" | sed -e 's%/compilers/.*$%%'`


### Init

if test $# -eq 1  -a  $x_action = "init"; then
    # Change script's command interpreter from /bin/sh to /bin/bash.
    # Cygwin's shell don't works correctly with process pids.
    # echo "Changing scripts command interpreter..."
    script_dirs="scripts"
    tmp="$x_tmp/check_make_win.$$"
    
    for d in $script_dirs; do
        script_list=`find $x_root_dir/$d -name '*.sh'`
        for s in $script_list; do
            echo $s | grep 'check_make_win.sh' > /dev/null 2>&1  &&  continue
            grep '^#! */bin/sh' $s > /dev/null 2>&1
            if test $? -eq 0; then
               cp -fp $s $tmp   ||  exit 2
               sed -e 's|^#! */bin/sh.*$|#! /bin/bash|' $s > $tmp  ||  exit 2
               chmod a+x $s
               touch -r $s $tmp ||  exit 2
               cp -fp $tmp $s   ||  exit 2
               rm -f $tmp       ||  exit 2
            fi
        done
    done
    exit 0;
fi


### Create

if test $# -lt 4; then
    echo "Usage: $0 <cmd> <solution> <static|dll> <32|64> <cfg> [build_dir]"  &&  exit 1
fi

# Check list
x_check_dir="$x_build_dir/$x_libdll/build/${x_solution}.check/$x_cfg"
if test ! -d "$x_check_dir"; then
   echo "Check directory \"$x_check_dir\" not found."
   exit 1 
fi
x_list="$x_check_dir/check.sh.list"
if test ! -f "$x_list"; then
   echo "Check list file \"$x_list\" not found."
   exit 1 
fi
x_out="$x_check_dir/check.sh"
x_script_name=`echo "$x_out" | sed -e 's%^.*/%%'`


# Determine signature of the build (only for automatic builds)
signature=""
if [ -n "$NCBI_AUTOMATED_BUILD" ]; then
   case "$COMPILER" in
      msvc7 ) signature="MSVC_710" ;;
      msvc8 ) signature="MSVC_800" ;;
      gcc   ) signature="GCC_344"  ;;
   esac
   signature="$signature-${x_cfg}"
   case "$x_cfg" in
      *DLL) signature="${signature}MT" ;;
      *MT ) ;;
      *   ) signature="${signature}ST" ;;
   esac
   signature="${signature}${x_libdll}${ARCH}--i386-pc-win${ARCH:-32}-${SRV_NAME}"
fi

# Clean up check directory
(cd "$x_check_dir"  &&  find "$x_check_dir" -maxdepth 1 -mindepth 1 -type d -print | xargs rm -rf)


#//////////////////////////////////////////////////////////////////////////

cat > $x_out <<EOF
#! /bin/sh

root_dir="$x_root_dir"
build_dir="$x_build_dir"
check_dir="$x_check_dir"
src_dir="\$root_dir/src"
build_tree="$x_libdll" 
build_cfg="$x_cfg" 
signature="$signature"

res_script="$x_out"
res_journal="\$res_script.journal"
res_log="\$res_script.log"
res_list="\$res_script.list"
res_concat="\$res_script.out"
res_concat_err="\$res_script.out_err"

is_db_load=false


##  Printout USAGE info and exit

Usage() {
   cat <<EOF_usage

USAGE:  $x_script_name {run | concat | concat_err}

 run         Run tests. Create output file ("*.out") for each test, 
             journal and log files. 
 concat      Concatenate all output files created during the last "run" 
             into one file "\$res_log".
 concat_err  Like 'concat', but output file "\$res_concat_err" 
             will contains outputs of failed tests only.

ERROR:  \$1
EOF_usage
# Undocumented command:
#     load_to_db  Load data about test to database keeping all statistics.

    exit 1
}

if test \$# -ne 1 ; then
   Usage "Invalid number of arguments."
fi


###  What to do (cmd-line arg)

method="\$1"


### Action

case "\$method" in
#----------------------------------------------------------
   run )
      # See RunTest() below
      ;;
#----------------------------------------------------------
   concat )
      rm -f "\$res_concat"
      ( 
      cat \$res_log
      test -f \$res_journal  ||  exit 0
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
      egrep '(ERR|TO) \[' \$res_log
      test -f \$res_journal  ||  exit 0
      x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
      for x_file in \$x_files; do
         x_file=\`echo "\$x_file" | sed -e 's/%gj_s4%/ /g'\`
         x_code=\`grep -c '@@@ EXIT CODE:' \$x_file\`
         test \$x_code -ne 0 || continue
         x_good=\`grep -c '@@@ EXIT CODE: 0' \$x_file\`
         if test \$x_good -ne 1 ; then
            echo 
            echo 
            cat \$x_file
         fi
      done
      ) >> \$res_concat_err
      exit 0
      ;;
#----------------------------------------------------------
   load_to_db )
      is_db_load=true
      # See RunTest() below
      ;;
#----------------------------------------------------------
   * )
      Usage "Invalid method name."
      ;;
esac


## Run

# Export some global vars
export top_srcdir="\$root_dir"

# Add current, build and scripts directories to PATH
export PATH=".:\${build_dir}:\${root_dir}/scripts/common/impl:\${PATH}"

# Define time-guard script to run tests from other scripts
export CHECK_EXEC="\${root_dir}/scripts/common/check/check_exec_test.sh"
export CHECK_EXEC_STDIN="\$CHECK_EXEC -stdin"

# Enable silent abort for NCBI applications on fatal errors
export DIAG_SILENT_ABORT="Y"


count_ok=0
count_err=0
count_absent=0
configurations="$x_confs"

if ! \$is_db_load; then
    rm -f "\$res_journal"
    rm -f "\$res_log"
fi

##  Run one test

RunTest() {
   # Parameters
   x_wdir="\$1"
   x_test="\$2"
   x_app="\$3"
   x_run="\${4:-\$x_app}"
   x_name="\${5:-\$x_run}"  
   x_real_name="\$5"
   x_ext="\$6"
   x_timeout="\$7"
 
   x_work_dir="\$check_dir/\$x_wdir"
   mkdir -p "\$x_work_dir" > /dev/null 2>&1 

   # Determine test application name
   x_path_run="\$build_dir/\${build_tree}/bin/\$build_cfg"
   result=1
   x_path_app="\$x_path_run/\$x_app"
   if test ! -f "\$x_path_app"; then
      x_path_app="\$x_path_run/\$x_test"
      if test ! -f "\$x_path_app"; then
         result=0
      fi
   fi

   # Generate name of the output file
   x_test_out="\$x_work_dir/\$x_app.out\$x_ext"
   x_test_rep="\$x_work_dir/\$x_app.rep\$x_ext"
   x_boost_rep="\$x_work_dir/\$x_app.boost_rep\$x_ext"

   if \$is_db_load; then
      test_stat_load "\$(cygpath -w "\$x_test_rep")" "\$(cygpath -w "\$x_test_out")" "\$(cygpath -w "\$x_boost_rep")" "\$(cygpath -w "\$top_srcdir/build_info")" >> "\$build_dir/test_stat_load.log" 2>&1
      return 0
   fi
   
   if [ -n "\$NCBI_AUTOMATED_BUILD" ]; then
      echo "\$signature"   >> "\$x_test_rep"
      echo "\$x_wdir"      >> "\$x_test_rep"
      echo "\$x_run"       >> "\$x_test_rep"
      echo "\$x_real_name" >> "\$x_test_rep"
      export NCBI_BOOST_REPORT_FILE="\$(cygpath -w "\$x_boost_rep")"
   fi

   x_cmd="[\$build_tree/\$build_cfg/\$x_wdir]"
##   x_cmd="[\$x_wdir]"

   if test \$result -eq 0; then
      echo "ABS --  \$x_cmd - \$x_test"
      echo "ABS --  \$x_cmd - \$x_test" >> \$res_log
      count_absent=\`expr \$count_absent + 1\`
      if [ -n "\$NCBI_AUTOMATED_BUILD" ]; then
         echo "ABS" >> "\$x_test_rep"
         echo "\`date +'$x_date_format'\`" >> "\$x_test_rep"
      fi
      return 0
   fi

   # Write header to output file 
   echo "\$x_test_out" >> \$res_journal
   (
       echo "======================================================================"
       echo "\$build_tree/\$build_cfg - \$x_name"
       echo "======================================================================"
       echo 
   ) > \$x_test_out 2>&1

   # Goto the work directory 
   cd "\$x_work_dir"

   # Fix empty parameters (replace "" to \"\", '' to \'\')
   x_run_fix=\`echo "\$x_run" | sed -e 's/""/\\\\\\\\\\"\\\\\\\\\\"/g' -e "s/''/\\\\\\\\\\'\\\\\\\\\\'/g"\`
   # Fix empty parameters (put each in '' or "")
   x_run_fix=\`echo "\$x_run" | sed -e 's/""/'"'&'/g" -e "s/''/\\\\'\\\\'/g"\`

   # Run check
   export CHECK_TIMEOUT="\$x_timeout"
   start_time="\`date +'$x_date_format'\`"
   check_exec="\$root_dir/scripts/common/check/check_exec.sh"
   \$check_exec time.exe -p \`eval echo \$x_run_fix\` > \$x_test_out.\$\$ 2>&1
   result=\$?
   stop_time="\`date +'$x_date_format'\`"

   sed -e '/ ["][$][@]["].*\$/ {
       s/^.*: //
       s/ ["][$][@]["].*$//
    }' \$x_test_out.\$\$ >> \$x_test_out

   # Get application execution time
   exec_time=\`tail -3 \$x_test_out.\$\$\ | tr '\r' '%'\`
   exec_time=\`echo "\$exec_time" | tr '\n' '%'\`
   echo \$exec_time | grep 'real [0-9]\|Maximum execution .* is exceeded' > /dev/null 2>&1 
   if [ \$? -eq 0 ] ;  then
      exec_time=\`echo \$exec_time | sed -e 's/^%*//' -e 's/%*$//' -e 's/%%/%/g' -e 's/%/, /g' -e 's/[ ] */ /g'\`
   else
      exec_time='unparsable timing stats'
   fi
   rm -f $x_tmp/\$\$.out
   rm -f \$x_test_out.\$\$

   # Write result of the test into the his output file
   echo "Start time   : \$start_time"   >> \$x_test_out
   echo "Stop time    : \$stop_time"    >> \$x_test_out
   echo >> \$x_test_out
   echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> \$x_test_out
   echo "@@@ EXIT CODE: \$result" >> \$x_test_out

   # And write result also on the screen and into the log
   x_cmd="\$x_cmd \$x_name"
   if grep -q NCBI_UNITTEST_DISABLED \$x_test_out >/dev/null; then
      echo "DIS --  \$x_cmd"
      echo "DIS --  \$x_cmd" >> \$res_log
      [ -n "\$NCBI_AUTOMATED_BUILD" ] && echo "DIS" >> "\$x_test_rep"
   elif grep NCBI_UNITTEST_SKIPPED \$x_test_out >/dev/null; then
      echo "SKP --  \$x_cmd"
      echo "SKP --  \$x_cmd" >> \$res_log

      [ -n "\$NCBI_AUTOMATED_BUILD" ] && echo "SKP" >> "\$x_test_rep"
   elif egrep "Maximum execution .* is exceeded" \$x_test_out >/dev/null; then
      echo "TO --  \$x_cmd"
      echo "TO --  \$x_cmd" >> \$res_log

      [ -n "\$NCBI_AUTOMATED_BUILD" ] && echo "TO" >> "\$x_test_rep"
   elif test \$result -eq 0; then
      echo "OK  --  \$x_cmd     (\$exec_time)"
      echo "OK  --  \$x_cmd     (\$exec_time)" >> \$res_log
      count_ok=\`expr \$count_ok + 1\`
      [ -n "\$NCBI_AUTOMATED_BUILD" ] && echo "OK" >> "\$x_test_rep"
   else
       echo "ERR [\$result] --  \$x_cmd     (\$exec_time)"
       echo "ERR [\$result] --  \$x_cmd     (\$exec_time)" >> \$res_log
       count_err=\`expr \$count_err + 1\`
       [ -n "\$NCBI_AUTOMATED_BUILD" ] && echo "ERR" >> "\$x_test_rep"
   fi
   if [ -n "\$NCBI_AUTOMATED_BUILD" ]; then
      echo "\$start_time" >> "\$x_test_rep"
      echo "\$result"     >> "\$x_test_rep"
      echo "\$exec_time"  >> "\$x_test_rep"
   fi
}

# Save value of PATH environment variable
saved_path="\$PATH"

# Features detection
fs=\`cat \${build_dir}/\${build_tree}/\${build_cfg}/features_and_packages.txt\ | tr '\r' ' '\`
FEATURES=""
for f in \$fs; do
   FEATURES="\$FEATURES\$f "
done
export FEATURES

# Add current configuration's build and dll build directories to PATH
export PATH=".:\${build_dir}/\${build_tree}/bin/\${build_cfg}:\${build_dir}/\${build_tree}/lib/\${build_cfg}:\${build_dir}/dll/bin/\${build_cfg}:\${saved_path}"

# Export bin and lib pathes
export CFG_BIN="\${build_dir}/\${build_tree}/bin/\${build_cfg}"
export CFG_LIB="\${build_dir}/\${build_tree}/lib/\${build_cfg}"


EOF

#//////////////////////////////////////////////////////////////////////////


# Read list with tests and write commands to script file.
# Also copy necessary files to the test build directory.


# Read list with tests
x_tests=`cat "$x_list" | sed -e 's/ /%gj_s4%/g'`
x_test_prev=""

# Features detection
features="${x_build_dir}/${x_libdll}/${x_cfg}/features_and_packages.txt"
   
# For all tests
for x_row in $x_tests; do
   # Get one row from list
   x_row=`echo "$x_row" | sed -e 's/%gj_s4%/ /g' -e 's/^ *//' -e 's/ ____ /~/g'`

   # Split it to parts
   x_rel_dir=`echo \$x_row | sed -e 's/~.*$//'`
   x_src_dir="$x_root_dir/src/$x_rel_dir"
   x_test=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_app=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_cmd=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//' -e 's/\"/\\\\"/g'`
   x_name=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_files=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_timeout=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_requires=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_authors=`echo "$x_row" | sed -e 's/.*~//'`

   # Check application requirements
   for x_req in $x_requires; do
      (grep "^$x_req$" $features > /dev/null)  ||  continue
   done
   
   # Copy specified files into the check tree
   if test ! -z "$x_files" ; then
      x_path="$x_check_dir/$x_rel_dir"
      mkdir -p "$x_path"
      for i in $x_files ; do
         x_copy="$x_src_dir/$i"
         if test -f "$x_copy"  -o  -d "$x_copy" ; then
            cp -prf "$x_copy" "$x_path"
            find "$x_path/$i" -name .svn -print | xargs rm -rf
         else
            echo "Warning:  \"$x_copy\" must be file or directory!"
         fi
      done
   fi
   
   # Generate extension for tests output file
   if test "$x_test" != "$x_test_prev" ; then 
      x_cnt=1
      x_test_ext=""
   else
      x_cnt=`expr $x_cnt + 1`
      x_test_ext="$x_cnt"
   fi
   x_test_prev="$x_test"

   # Write current test commands into a script file
   cat >> $x_out <<EOF
######################################################################
RunTest "$x_rel_dir" \\
        "$x_test" \\
        "$x_app" \\
        "$x_cmd" \\
        "$x_name" \\
        "$x_test_ext" \\
        "$x_timeout"
EOF

#//////////////////////////////////////////////////////////////////////////

done # for x_row in x_tests


# Write ending code into the script 
cat >> $x_out <<EOF
######################################################################


# Restore saved PATH environment variable
PATH="\$saved_path"

# Write result of the tests execution
echo
echo "Succeeded : \$count_ok"
echo "Failed    : \$count_err"
echo "Absent    : \$count_absent"
echo

if test \$count_err -eq 0; then
   echo
   echo "******** ALL TESTS COMPLETED SUCCESSFULLY ********"
   echo
fi

exit \$count_err
EOF

# Set execute mode to script
chmod a+x "$x_out"

exit 0
