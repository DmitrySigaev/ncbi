#!/bin/sh
errors=''
ok="0,0,0,0,0,0,0"
function run () {
	echo -- "$@" >&2
	"$@"
	return $?
}
./oligofar -V
errors="${errors}$?"
run ./oligofar -T+ -i NM_012345.reads -d NM_012345.fa -o NM_012345.reads.out -w9 -n1 -L2G "$@" 
errors="${errors},$?"
diff NM_012345.reads.ref NM_012345.reads.out > NM_012345.reads.diff
errors="${errors},$?"
run ./oligofar -T+ -i NM_012345.pairs -d NM_012345.fa -o NM_012345.pairs.out -w9 -n1 -L2G "$@"
errors="${errors},$?"
diff NM_012345.pairs.ref NM_012345.pairs.out > NM_012345.pairs.diff
errors="${errors},$?"
x=$(run ./oligofar -i test1.reads -d NM_012345.fa -Ou -n2 -e1 | tee test1.reads.out | awk '{print $14}' | sort | uniq -c)
test "$x" = '     30 AGCCTTCCTCTGAGTCGCAGCCCCCCATGGAGGCCCAGTCT'
errors="${errors},$?"
x=$(run ./oligofar -i test2.reads -d NM_012345.fa -Ou -S6 -n2 -e1 | tee test2.reads.out | awk '{print $6}' | sort | uniq -c)
test "$x" = '     22 100'
errors="${errors},$?"
if test "$errors" == "$ok" ; then
	rm NM_012345.reads.{out,diff} NM_012345.pairs.{out,diff}
else
	head NM_012345.{reads,pairs}.diff
fi
echo "ERRORS=$errors";
test "$errors" == "$ok"
