#! /bin/sh
# $Id$

# DBLIB does not work (on Linux at least) when this limit is > 1024
# CTLIB does not work on Solaris sparc when this limit is < ~1300
ulimit -n 1536 > /dev/null 2>&1


driver_list="ctlib dblib ftds ftds63 odbc msdblib"
# server_list="MS_DEV2 BARTOK BARTOK_12"
server_list="MS_DEV1 STRAUSS MOZART"
# server_mssql="MS_DEV2"
server_mssql="MS_DEV1"

res_file="/tmp/$0.$$"
trap 'rm -f $res_file' 1 2 15

n_ok=0
n_err=0
sum_list=""
driver_status=0

# Run one test
RunSimpleTest()
{
    # echo
    (
        cd $1 > /dev/null 2>&1
        # $CHECK_EXEC run_sybase_app.sh $cmd > $res_file 2>&1
        $CHECK_EXEC run_sybase_app.sh $cmd > /dev/null 2> $res_file
    )

    if test $? -eq 0 ; then
        # echo "OK:"
        n_ok=`expr $n_ok + 1`
        sum_list="$sum_list XXX_SEPARATOR +  $cmd"
        return
    fi

    # error occurred
    n_err=`expr $n_err + 1`
    sum_list="$sum_list XXX_SEPARATOR -  $cmd"

    echo
    cat $res_file
}

# Run one test (RunTest sql_command reg_expression)
RunTest()
{
    sql="$1"
    reg_exp="$2"
    
    echo
    $CHECK_EXEC run_sybase_app.sh $cmd "$sql" > $res_file 2>&1
    
    if test $? -eq 0 ; then
        if grep "$reg_exp" $res_file > /dev/null 2>&1 ; then
        echo "OK:"
        grep "$reg_exp" $res_file
        n_ok=`expr $n_ok + 1`
        sum_list="$sum_list XXX_SEPARATOR +  $cmd '$sql'"
        return
        fi
    fi

    # error occurred
    n_err=`expr $n_err + 1`
    sum_list="$sum_list XXX_SEPARATOR -  $cmd '$sql'"

    cat $res_file
}

# Run one test (RunTest sql_command)
RunTest2()
{
    sql="$1"

    # echo
    $CHECK_EXEC run_sybase_app.sh $cmd "$sql" > /dev/null 2> $res_file 

    if test $? -eq 0 ; then
        # echo "OK:"
        n_ok=`expr $n_ok + 1`
        sum_list="$sum_list XXX_SEPARATOR +  $cmd '$sql'"
        return
    fi

    # error occurred
    n_err=`expr $n_err + 1`
    sum_list="$sum_list XXX_SEPARATOR -  $cmd '$sql'"

    echo
    cat $res_file
}


# Check existence of the "dbapi_driver_check"
$CHECK_EXEC run_sybase_app.sh dbapi_driver_check
if test $? -ne 99 ; then
    echo "The DBAPI driver existence check application not found."
    echo
    exit 1
fi


# Loop through all combinations of {driver, server, test}
for driver in $driver_list ; do
    cat <<EOF

******************* DRIVER:  $driver ************************
EOF


    $CHECK_EXEC run_sybase_app.sh dbapi_driver_check $driver
    driver_status=$?

    if test $driver_status -eq 5; then 
        cat <<EOF

Driver not found.
EOF
    elif test $driver_status -eq 4; then
        n_err=`expr $n_err + 1`
        sum_list="$sum_list XXX_SEPARATOR -  dbapi_driver_check $driver (Database-related driver initialization error)"
        cat <<EOF
    
Database-related driver initialization error.
EOF
    elif test $driver_status -eq 3; then
        n_err=`expr $n_err + 1`
        sum_list="$sum_list XXX_SEPARATOR -  dbapi_driver_check $driver (Corelib-related driver initialization error)"
        cat <<EOF

Corelib-related driver initialization error.
EOF
    elif test $driver_status -eq 2; then
        n_err=`expr $n_err + 1`
        sum_list="$sum_list XXX_SEPARATOR -  dbapi_driver_check $driver (C++-related driver initialization error)"
        cat <<EOF

C++-related driver initialization error.
EOF
    elif test $driver_status -eq 1; then
        n_err=`expr $n_err + 1`
        sum_list="$sum_list XXX_SEPARATOR -  dbapi_driver_check $driver (Unknown driver initialization error)"
        cat <<EOF

Unknown driver initialization error.
EOF

    else
        for server in $server_list ; do
            if test $driver = "ctlib"  -a  $server = $server_mssql ; then
                continue
            fi

            cat <<EOF

~~~~~~ SERVER:  $server ~~~~~~~~~~~~~~~~~~~~~~~~
EOF

        # lang_query
            cmd="lang_query -d $driver -S $server -Q"
            RunTest2 'select qq = 57.55 + 0.0033' '<ROW><qq>57\.5533<'
            RunTest2 'select qq = 57 + 33' '<ROW><qq>90<'
            RunTest2 'select qq = GETDATE()' '<ROW><qq>../../.... ..:..:..<'
            RunTest2 'select name, type from sysobjects' '<ROW><name>'
        # simple tests
            if test $driver = "dblib"  -a  $server = $server_mssql ; then
                continue
            fi
            # do not run tests wit a boolk copy operations 
            # on Sybase databases with the "ftds" driver
            if test \( $driver = "ftds" -a $server = $server_mssql \) \
                    -o $driver != "ftds" ; then
                cmd="dbapi_bcp -d $driver -S $server"
                RunSimpleTest "dbapi_bcp"
                cmd="dbapi_testspeed -d $driver -S $server"
                RunSimpleTest "dbapi_testspeed"
            fi
            # exclude "dbapi_cursor" from testing MS SQL with the "ftds" driver
            if test $driver != "ftds" -a $server != $server_mssql ; then
                cmd="dbapi_cursor -d $driver -S $server"
                RunSimpleTest "dbapi_cursor"
            fi
            cmd="dbapi_query -d $driver -S $server"
            RunSimpleTest "dbapi_query"
            cmd="dbapi_send_data -d $driver -S $server"
            RunSimpleTest "dbapi_send_data"

        done

    fi

done

rm -f $res_file


# Print summary
cat <<EOF


*******************************************************

SUCCEEDED:  $n_ok
FAILED:     $n_err
EOF
echo "$sum_list" | sed 's/XXX_SEPARATOR/\
/g'


# Exit
exit $n_err
