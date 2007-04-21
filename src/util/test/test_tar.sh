#! /bin/sh

# Favor GNU tar if available, as vendor versions may have issues with
# corner cases.
if gtar --version >/dev/null 2>&1; then
  tar=gtar
else
  tar=tar
fi

mkdir /tmp/test_tar.$$.1  ||  exit 1
trap 'rm -rf /tmp/test_tar.$$.* &' 0 1 2 15

cp -rp . /tmp/test_tar.$$.1/ 2>/dev/null

mkdir /tmp/test_tar.$$.1/testdir.$$ 2>/dev/null

date >/tmp/test_tar.$$.1/testdir.$$/datefile 2>/dev/null

ln -s /tmp/test_tar.$$.1/testdir.$$/datefile  /tmp/test_tar.$$.1/testdir.$$/12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

touch /tmp/test_tar.$$.1/12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

(cd /tmp/test_tar.$$.1  &&  $tar cvf /tmp/test_tar.$$.tar .)       ||  exit 1

test_tar -T -f /tmp/test_tar.$$.tar                                ||  exit 1

mkdir /tmp/test_tar.$$.2                                           ||  exit 1

cat /tmp/test_tar.$$.tar | test_tar -C /tmp/test_tar.$$.2 -x -f -  ||  exit 1

diff -r /tmp/test_tar.$$.1 /tmp/test_tar.$$.2 2>/dev/null          ||  exit 1

test_tar -C /tmp/test_tar.$$.2 -c -f - . 2>/dev/null | $tar tvf -  ||  exit 1

exit 0
