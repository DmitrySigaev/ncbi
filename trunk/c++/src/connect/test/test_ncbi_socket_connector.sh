#! /bin/sh
# $Id$

outlog()
{
  logfile="$1"
  if [ -s "$logfile" ]; then
    echo "=== $logfile ==="
    if [ "`cat $logfile 2>/dev/null | wc -l`" -gt "200" ]; then
      head -100 "$logfile"
      echo '...'
      tail -100 "$logfile"
    else
      cat "$logfile"
    fi
  fi
}

exit_code=0
server_log=socket_io_bouncer.log
client_log=test_ncbi_socket_connector.log

rm -f $server_log $client_log

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

port="575`expr $$ % 100`"

socket_io_bouncer $port >$server_log 2>&1 &
spid=$!
trap 'kill -9 $spid' 0 1 2 15

sleep 1
$CHECK_EXEC test_ncbi_socket_connector localhost $port >client_log 2>&1  ||  exit_code=1

kill $spid  ||  exit_code=2
( kill -9 $spid ) >/dev/null 2>&1

if [ $exit_code != 0 ]; then
  outlog "$server_log"
  outlog "$client_log"
fi

exit $exit_code
