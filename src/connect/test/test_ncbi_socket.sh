#! /bin/sh
# $Id$

exit_code=0
port="555`expr $$ % 100`"

./test_ncbi_socket $port >/dev/null 2>&1 &
server_pid=$!
trap 'kill -9 $server_pid' 1 2 15

sleep 1
./test_ncbi_socket localhost $port  ||  exit_code=1

kill $server_pid  ||  exit_code=2
( kill -9 $server_pid ) >/dev/null 2>&1
exit $exit_code
