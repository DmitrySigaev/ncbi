#!/bin/sh
file=$1.lock

user=$REMOTE_USER
[ -z "$user" ] && user=$USER
[ -z "$user" ] && user=`id`

fmt -79 > "$file.$$" <<EOF
$user appears to be running $1 in `pwd` as process $2 on `hostname`.  If
you have received this message in error, delete `pwd`/$file and try again.
EOF

# echo no | mv -i "$file.$$" "$file" # Doesn't work on Solaris. :-/
[ -f "$file" ] || mv "$file.$$" "$file" # Not atomic. :-/

if [ -f "$file.$$" ]; then
    cat "$file" >& 2
    rm "$file.$$"
#    kill $2
    exit 1
fi
