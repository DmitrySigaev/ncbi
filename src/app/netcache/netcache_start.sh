#!/bin/sh
#
# $Id$
#
# Author: Anatoliy Kuznetsov
#
#  Check if netcached is not running and run it

start_dir=`pwd`

Die() {
    echo "$@" >& 2
    cd $start_dir
    exit 1
}

Success() {
    echo "$@"
    echo $nc_pid > netcached.pid    
    cat netcached.out | mail -s "$@" $mail_to    
    cd $start_dir
    exit 0
}

# ---------------------------------------------------- 


run_dir=$1
ini_file=netcached.ini
BIN_PATH=
#~/cxx/GCC340-ReleaseMT/build/app/netcache/
nc_control=${BIN_PATH}netcache_control
netcached=${BIN_PATH}netcached
service_wait=10
mail_to="kuznets@ncbi -c vakatov@ncbi"



if [ ! -d "$run_dir" ]; then
    Die "Startup directory ( $run_dir ) does not exist"
fi

cd $run_dir

if [ ! -f $ini_file ]; then
    Die "Cannot find $ini_file at  $run_dir"
fi

host=`hostname`
port=`cat netcached.ini | grep port= | sed -e 's/port=//'`

echo "Testing if netcached is alive on $host:$port"


if ! $nc_control -v $host $port > /dev/null  2>&1; then
    echo "Service not responding"
    
    echo "Starting the netcached service..."
    cat netcached.out >> netcached_out.old
    $netcached > netcached.out  2>&1 &
    nc_pid=$!
    echo "Waiting for the service to start ($service_wait seconds)..."
    sleep $service_wait
    
    if ! $nc_control -v $host $port > /dev/null  2>&1; then
        echo "Service failed to start in $service_wait seconds"
        
        echo "Giving it $service_wait seconds more..."
        sleep $service_wait
        
        if ! $nc_control -v $host $port > /dev/null  2>&1; then
            cat netcached.out | mail -s "[PROBLEM] netcached @ $host:$port failed to start" $mail_to
            
            kill $nc_pid
            sleep 3
            kill -9 $nc_pid
            
            echo "Database reinitialization."
            
            $netcached -reinit >> netcached.out  2>&1 &
            nc_pid=$!
            
            echo "Waiting for the service to start ($service_wait seconds)..."
            sleep $service_wait
            
            if ! $nc_control -v $host $port > /dev/null  2>&1; then
                echo "Service failed to start with database reinitialization"
                cat netcached.out | mail -s "[PROBLEM] netcached @ $host:$port failed to start with -reinit" $mail_to
                Die "Failed to start service"
            else
                Success "netcached started with -reinit (pid=$nc_pid) at $host:$port"
            fi                        
        fi        
    fi
else
    echo "Service is alive"
    cd $start_dir
    exit 0
fi

Success "netcached started (pid=$nc_pid) at $host:$port"

cd $start_dir
