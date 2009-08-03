#!/bin/sh

INSTALLDIR=$1
SCRIPTDIR=$2
BLAST_VERSION=$3
PRODUCT="ncbi-blast-$BLAST_VERSION+"

if [ $# -ne 3 ] ; then
    echo "Usage: ncbi-blast.sh [installation directory] [MacOSX post-build script directory] [BLAST version]";
    exit 1;
fi

BLAST_BINS="blastn blastp blastx tblastn tblastx psiblast rpsblast rpstblastn blast_formatter legacy_blast.pl update_blastdb.pl"
MASKING_BINS="windowmasker dustmasker segmasker"
DB_BINS="blastdbcmd makeblastdb makembindex blastdb_aliastool convert2blastmask blastdbcheck"
ALL_BINS="$BLAST_BINS $MASKING_BINS $DB_BINS"

rm -rf $PRODUCT.dmg $PRODUCT _stage $INSTALLDIR/installer
mkdir -p _stage/usr/local/ncbi/blast/bin _stage/usr/local/ncbi/blast/doc _stage/private/etc/paths.d

ftp -o _stage/usr/local/ncbi/blast/doc/user_manual.pdf "http://web.ncbi.nlm.nih.gov/bookshelf/picrender.fcgi?book=helpblast&&partid=1763&blobtype=pdf"

cp -p $SCRIPTDIR/ncbi_blast _stage/private/etc/paths.d

# This is needed because the binary ncbi-blast.pmproj has this string hard
# coded
cp -p $INSTALLDIR/LICENSE ./license.txt
for f in uninstall_ncbi_blast.zip large-Blue_ncbi_logo.tiff ncbi-blast.pmdoc welcome.txt; do
    echo copying $f to local directory
    cp -rp $SCRIPTDIR/$f .
done

for bin in $ALL_BINS; do
    echo copying $bin
    cp -p $INSTALLDIR/bin/$bin _stage/usr/local/ncbi/blast/bin
done

echo building package
mkdir $PRODUCT
/Developer/usr/bin/packagemaker --id gov.nih.nlm.ncbi.blast --doc ncbi-blast.pmdoc --out $PRODUCT/$PRODUCT.pkg

echo copying uninstaller
cp -p uninstall_ncbi_blast.zip $PRODUCT

echo creating disk image
/usr/bin/hdiutil create $PRODUCT.dmg -srcfolder $PRODUCT

echo moving disk image
mkdir $INSTALLDIR/installer
mv $PRODUCT.dmg $INSTALLDIR/installer

echo done
rm -rf _stage $PRODUCT
