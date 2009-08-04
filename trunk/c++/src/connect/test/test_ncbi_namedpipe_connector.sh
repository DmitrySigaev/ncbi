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
client_log=test_ncbi_namedpipe_connector_client.log
server_log=test_ncbi_namedpipe_connector_server.log

rm -f $client_log $server_log

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

test_ncbi_namedpipe_connector server >>$server_log 2>&1 &

spid=$!
trap 'kill -9 $spid; rm -f ./.test_ncbi_namedpipe' 0 1 2 15

sleep 10
$CHECK_EXEC test_ncbi_namedpipe_connector client >>$client_log 2>&1  ||  exit_code=1

kill $spid  ||  exit_code=2
( kill -9 $spid ) >/dev/null 2>&1

if [ $exit_code != 0 ]; then
  outlog "$client_log"
  outlog "$server_log"
fi

rm -f ./.test_ncbi_namedpipe >/dev/null 2>&1

exit $exit_code
