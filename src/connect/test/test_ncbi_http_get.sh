#! /bin/sh
#$Id$

if [ "`echo $FEATURES | grep -wic GNUTLS`" = "1" ]; then
    CONN_USESSL=1; export CONN_USESSL
    url='https://test.gnutls.org:5556'
else
    url='http://www.ncbi.nlm.nih.gov/entrez/viewer.cgi?view=0&maxplex=1&save=idf&val=4959943'
fi
exec test_ncbi_http_get "$url"
