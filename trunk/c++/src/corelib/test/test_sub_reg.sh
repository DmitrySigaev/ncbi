#!/bin/sh
set -e # exit immediately if any commands fail

NCBI_CONFIG_PATH=`dirname $0`/test_sub_reg_data
NCBI_CONFIG_OVERRIDES=$NCBI_CONFIG_PATH/indirect_env.ini
NCBI_CONFIG_e__test=env
NCBI_CONFIG__test__e=env
NCBI_CONFIG__test__e_ie=env
NCBI_CONFIG__test__ob_e=env
NCBI_CONFIG__test__obx_e=env
NCBI_CONFIG__a_DOT_b__c_DOT_d=e.f
export NCBI_CONFIG_PATH NCBI_CONFIG_OVERRIDES NCBI_CONFIG_e__test
export NCBI_CONFIG__test__e NCBI_CONFIG__test__e_ie NCBI_CONFIG__test__ob_e
export NCBI_CONFIG__test__obx_e NCBI_CONFIG__a_DOT_b__c_DOT_d

$CHECK_EXEC test_sub_reg -defaults "$NCBI_CONFIG_PATH/defaults.ini" \
                         -overrides "$NCBI_CONFIG_PATH/overrides.ini" \
                         -out test_sub_reg.out
diff $NCBI_CONFIG_PATH/expected.ini test_sub_reg.out

echo Test passed -- no differences encountered.
exit 0
