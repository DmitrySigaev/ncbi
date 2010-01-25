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
server_log=test_ncbi_namedpipe_server.log
client_log=test_ncbi_namedpipe_client.log

rm -f $server_log $client_log

test_ncbi_namedpipe -suffix $$ server >$server_log 2>&1 &
spid=$!
trap 'kill -0 $spid && kill -9 $spid; rm -f ./.ncbi_test_pipename_$$' 0 1 2 15

sleep 2
$CHECK_EXEC test_ncbi_namedpipe -suffix $$ client >$client_log  ||  exit_code=1

kill $spid  ||  exit_code=2
( kill -9 $spid ) >/dev/null 2>&1

rm -f ./.ncbi_test_pipename_$$ >/dev/null 2>&1

if [ $exit_code != 0 ]; then
  outlog "$server_log"
  outlog "$client_log"
  uptime
fi

exit $exit_code
