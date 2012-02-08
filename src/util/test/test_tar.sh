#! /bin/sh
# $Id$

# Locate native tar;  exit successfully if none found
string="`whereis tar 2>/dev/null`"
if [ -z "$string" ]; then
  tar="`which tar 2>/dev/null`"
else
  tar="`echo $string | cut -f2 -d' '`"
fi
test -x "$tar"  ||  exit 0

# Figure out whether the API is lf64 clean, also exclude notoriously slow platforms
okay=false
what='not supported'
case ${CHECK_SIGNATURE:-Unknown} in

  GCC* )
    # Older GCC builds are not lf64 clean
    test `echo $CHECK_SIGNATURE | sed 's/^[^_]*_//;s/-.*$//'` -ge 340  &&  okay=true
    ;;

  WorkShop* )
    # Although 64-bit build should be okay, it is very slow (because RTL
    # has a file positioning bug, the archive has to be read out seqentially,
    # which prevents from using faster direct access file seeks)...
    what='slow'
    ;;

  * )
    # NB: Here falls ICC
    test_tar -lfs -f -  &&  okay=true
    ;;

esac

huge_tar="/am/ftp-geo/DATA/supplementary/series/GSE15735/GSE15735_RAW.tar"
if [ -f "$huge_tar" ]; then
  if $okay ; then
    echo
    echo "`date` *** Checking compatibility with existing NCBI data"
    echo

    test_tar -T -f "$huge_tar"  ||  exit 1
  else
    echo
    echo "`date` *** LF64 ${what}, skipping data compatibility test"
  fi
fi

echo
echo "`date` *** Preparing file staging area"
echo

test "`uname | grep -ic '^cygwin'`" != "0"  &&  exe=".exe"

test_base="${TMPDIR:-/tmp}/test_tar.$$"
mkdir $test_base.1  ||  exit 1
trap 'rm -rf $test_base* & echo "`date`."' 0 1 2 15

cp -rp . $test_base.1/ 2>/dev/null
if [ -f test_tar${exe} ]; then
  test_exe=./test_tar${exe}
else
  test_exe=$CFG_BIN/test_tar${exe}
  cp -p $test_exe $test_base.1/ 2>/dev/null
fi

mkdir $test_base.1/testdir 2>/dev/null

mkfifo -m 0567 $test_base.1/.testfifo 2>/dev/null

date >$test_base.1/testdir/datefile 2>/dev/null

ln -s $test_base.1/testdir/datefile $test_base.1/testdir/ABS12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

ln -s           ../testdir/datefile $test_base.1/testdir/REL12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

ln    $test_base.1/testdir/datefile $test_base.1/testdir/linkfile 2>/dev/null

mkdir $test_base.1/testdir/12345678901234567890123456789012345678901234567890 2>/dev/null

touch $test_base.1/testdir/12345678901234567890123456789012345678901234567890/12345678901234567890123456789012345678901234567890 2>/dev/null

touch $test_base.1/12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

ln -s $test_base.1/12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 $test_base.1/LINK12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 2>/dev/null

echo "`date` *** Creating test archive using native tar utility"
echo

( cd $test_base.1  &&  $tar cvf $test_base.tar . )

rm -rf $test_base.1
mkdir  $test_base.1                                                                     ||  exit 1

echo
echo "`date` *** Sanitizing test archive from unsupported features"
echo

( cd $test_base.1  &&  $tar  xf $test_base.tar )

echo "`date` *** Checking the resultant archive"
echo

dd if=$test_base.tar bs=123 2>/dev/null | test_tar -T -f -                              ||  exit 1

sleep 1
mkdir             $test_base.1/newdir 2>/dev/null
chmod g+s,o+t,o-x $test_base.1/newdir 2>/dev/null
date 2>/dev/null | tee -a $test_base.1/testdir.$$/datefile $test_base.1/newdir/datefile >$test_base.1/datefile 2>/dev/null
cp -fp $test_base.1/newdir/datefile $test_base.1/newdir/dummyfile 2>/dev/null

echo
echo "`date` *** Checking simple update"
echo

test_tar -C $test_base.1 -u -v -f $test_base.tar ./datefile ./newdir                    ||  exit 1

mv -f $test_base.1/datefile $test_base.1/phonyfile 2>/dev/null
mkdir $test_base.1/datefile 2>/dev/null

sleep 1
date >>$test_base.1/newdir/datefile 2>/dev/null

echo
echo "`date` *** Checking incremental update"
echo

test_tar -C $test_base.1 -u -U -v -E -f $test_base.tar ./newdir ./datefile ./phonyfile  ||  exit 1

rmdir $test_base.1/datefile 2>/dev/null
mv -f $test_base.1/phonyfile $test_base.1/datefile 2>/dev/null

mkdir $test_base.2                                                                      ||  exit 1

echo
echo "`date` *** Checking piping in and extraction"
echo

dd if=$test_base.tar bs=4567 | test_tar -C $test_base.2 -v -x -f -                      ||  exit 1
rm -f $test_base.1/.testfifo $test_base.2/.testfifo
diff -r $test_base.1 $test_base.2 2>/dev/null                                           ||  exit 1

echo
echo "`date` *** Checking piping out and compatibility with native tar utility"
echo

mkfifo -m 0567 $test_base.2/.testfifo >/dev/null 2>&1
test_tar -C $test_base.2 -c -f - . 2>/dev/null | $tar tBvf -                            ||  exit 1

echo
echo "`date` *** Checking safe extraction implementation"
echo

test_tar -C $test_base.2 -x    -f $test_base.tar                                        ||  exit 1

echo "`date` *** Checking backup feature"
echo

test_tar -C $test_base.2 -x -B -f $test_base.tar '*testdir/?*'                          ||  exit 1

echo '+++ Files:'
echo $test_base.2/testdir/* | tr ' ' '\n' | grep -v '[.]bak'
echo
echo '+++ Backups:'
echo $test_base.2/testdir/* | tr ' ' '\n' | grep    '[.]bak'
echo
files="`echo $test_base.2/testdir/* | tr ' ' '\n' | grep -v -c '[.]bak'`"
bkups="`echo $test_base.2/testdir/* | tr ' ' '\n' | grep    -c '[.]bak'`"
echo "+++ Files: $files --- Backups: $bkups"
test _"$files" = _"$bkups"                                                              ||  exit 1

echo
echo "`date` *** Checking single entry streaming feature"
echo

test_tar -x -s -v -f $test_base.tar "*test_tar${exe}" | cmp -l - $test_exe              ||  exit 1

echo "`date` *** Checking multiple entry streaming feature"
echo

test_tar -x -s -v -f $test_base.tar "*test_tar${exe}" newdir/datefile newdir/datefile > $test_base.out.1   ||  exit 1
head -1 "$test_base.2/newdir/datefile" > "$test_base.out.temp"                                             ||  exit 1
cat $test_exe $test_base.out.temp $test_base.2/newdir/datefile                        > $test_base.out.2   ||  exit 1
cmp -l $test_base.out.1 $test_base.out.2                                                                   ||  exit 1

echo
echo "`date` *** Checking in-stream append"
echo

touch $test_base.cat $test_base.dog
test_tar -c -v -f - $test_base.cat $test_base.dog | tar tvf - | tee $test_base.lst      ||  exit 1
test `cat $test_base.lst | wc -l` = 2                                                   ||  exit 1

echo
echo "`date` *** Checking stream append"
echo

test_tar -r -s -v -f $test_base.tar $test_base.out.1 $test_base.out.2                   ||  exit 1
test_tar -T       -f $test_base.tar                                                     ||  exit 1

if [ "`uname`" = "Linux" ]; then
  # Note that at least gtar 1.15.1 suffers from the following shortcoming:
  # when adding to an archive, gtar resets archive format using the last header
  # seen, so even if there was an old GNU archive, to which ustar entries
  # have later been appended, the --format=gnu won't have any effect on any
  # following added entries because it'll be reset by the last "ustar" entry
  # (seen when the archive gets scanned for an append) so all the newly
  # added entries will always be "ustar".  I consider this as a bug.

  $tar --help 2>&1 | grep -q -- '-H'  &&  format="--format=gnu -"

  prebs="`expr $$ % 10000 + 1`"
  spabs="`expr $$ % 1000  + 1`"
  posbs="`expr $$ % 10240 + 1`"
  nseek="`expr 1024000 / $spabs`"

  echo
  echo "`date` *** Checking sparse file tolerance"
  echo

  dd of=$test_base.1/newdir/pre-sparse  bs="$prebs" count=1                            if=/dev/urandom  ||  exit 1
  dd of=$test_base.1/newdir/sparse-file bs="$spabs" count=1 seek="$nseek"              if=/dev/urandom  ||  exit 1
  nseek="`expr $nseek '*' 2`"
  dd of=$test_base.1/newdir/sparse-file bs="$spabs" count=1 seek="$nseek" conv=notrunc if=/dev/urandom  ||  exit 1
  nseek="`expr $nseek '*' 2`"
  dd of=$test_base.1/newdir/sparse-file bs="$spabs" count=1 seek="$nseek" conv=notrunc if=/dev/urandom  ||  exit 1
  nseek="`expr $nseek '*' 2`"
  dd of=$test_base.1/newdir/sparse-file bs="$spabs" count=1 seek="$nseek" conv=notrunc if=/dev/urandom  ||  exit 1
  nseek="`expr $nseek '*' 2`"
  dd of=$test_base.1/newdir/sparse-file bs="$spabs" count=1 seek="$nseek" conv=notrunc if=/dev/urandom  ||  exit 1
  nseek="`expr $nseek '*' 2`"
  dd of=$test_base.1/newdir/sparse-file bs="$spabs" count=1 seek="$nseek" conv=notrunc if=/dev/urandom  ||  exit 1
  dd of=$test_base.1/newdir/post-sparse bs="$posbs" count=1                            if=/dev/urandom  ||  exit 1
  ls -l $test_base.1/newdir/*sparse*

  ( cd $test_base.1/newdir  &&  $tar ${format}Srvf $test_base.tar pre-sparse sparse-file post-sparse )  ||  exit 1

  test_tar -T -f $test_base.tar                                                                         ||  exit 1

  if $okay ; then
    free="`df -k /tmp | tail -1 | sed 's/  */ /g' | cut -f 4 -d ' '`"
    if [ "$free" -gt "4200000" ]; then
      echo
      echo "`date` *** ${free}KiB available in /tmp:  Checking 4GiB barrier"
      echo

      dd of=$test_base.1/newdir/huge-file bs=1 count="`expr $$ % 10000`" seek=4G if=/dev/urandom        ||  exit 1
      real="`ls -l $test_base.1/newdir/huge-file | tail -1 | sed 's/  */ /g' | cut -f 5 -d ' '`"
      test_tar -r -v -f $test_base.tar -C $test_base.1/newdir pre-sparse huge-file post-sparse          ||  exit 1
      size="`test_tar -t -v -f $test_base.tar huge-file 2>&1 | grep huge-file | tail -1 | sed 's/  */ /g' | cut -f 3 -d ' '`"

      if [ "$size" != "$real" ]; then
        echo "--- Entry size mismatch: $size is expected to be $real"
        exit 1
      fi
      size="`ls -l $test_base.tar | tail -1 | sed 's/  */ /g' | cut -f 5 -d ' '`"
      if [ "$size" -le "$real" ]; then
        echo "--- Archive size mismatch: $size is expected to be greater than $real"
        exit 1
      fi

      $tar tvf $test_base.tar                                                                           ||  exit 1
    fi
  fi
fi

echo
echo "`date` *** TEST COMPLETE"

exit 0
