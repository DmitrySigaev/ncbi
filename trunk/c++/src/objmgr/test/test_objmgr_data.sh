#! /bin/sh
#$Id$

for args in "" "-fromgi 30240900 -togi 30241000"; do
    echo "Testing: ./test_objmgr_data $args"
    if ! time ./test_objmgr_data $args; then
        exit 1
    fi
done
exit 0
