#!/bin/sh
set -e # exit immediately if any commands fail

if diff --help 2>/dev/null | grep -e '--strip-trailing-cr' >/dev/null; then
    flags=--strip-trailing-cr
else
    flags=
fi

for x in `printenv | sed -ne 's/^\(NCBI_CONFIG_[^=]*\)=.*/\1/p'`; do
    echo "Clearing $x setting."
    unset $x
done

export NCBI_CONFIG_PATH=`dirname $0`/test_sub_reg_data
export NCBI_CONFIG_OVERRIDES=$NCBI_CONFIG_PATH/indirect_env.ini
export NCBI_CONFIG_e__test=env
export NCBI_CONFIG__test__e=env
export NCBI_CONFIG__test__ex=
export NCBI_CONFIG__environment_DOT_indirectenv__e_ie=env
export NCBI_CONFIG__environment_DOT_indirectenv__ex_ie=
export NCBI_CONFIG__overridesbase_DOT_environment__ob_e=env
export NCBI_CONFIG__overridesbase_DOT_environment__obx_e=env
export NCBI_CONFIG__a_DOT_b__c_DOT_d=e.f

$CHECK_EXEC test_sub_reg -defaults "$NCBI_CONFIG_PATH/defaults.ini" \
                         -overrides "$NCBI_CONFIG_PATH/overrides.ini" \
                         -out test_sub_reg_1strun.out.ini
echo "Comparing first run and expected output"
unamestr=`uname -s`
if [[ "$unamestr" == MINGW* ]] || [[ "$unamestr" == CYGWIN* ]]; then
    echo "Operating system is Windows"
    diff $flags $NCBI_CONFIG_PATH/expected_win.ini test_sub_reg_1strun.out.ini
else
    echo "Operating system is not Windows"
    diff $flags $NCBI_CONFIG_PATH/expected.ini test_sub_reg_1strun.out.ini
fi

# Second run

unset NCBI_CONFIG_PATH 
unset NCBI_CONFIG_OVERRIDES NCBI_CONFIG_e__test
unset NCBI_CONFIG__test__e NCBI_CONFIG__test__ex 
unset NCBI_CONFIG__environment_DOT_indirectenv__ex_ie NCBI_CONFIG__overridesbase_DOT_environment__ob_e NCBI_CONFIG__overridesbase_DOT_environment__obx_e 
unset NCBI_CONFIG__environment_DOT_indirectenv__e_ie NCBI_CONFIG__a_DOT_b__c_DOT_d

cp test_sub_reg_1strun.out.ini test_sub_reg.ini

$CHECK_EXEC test_sub_reg -out test_sub_reg_2ndrun.out.ini
sort test_sub_reg_1strun.out.ini -o test_sub_reg_1strun.sorted.out.ini
sort test_sub_reg_2ndrun.out.ini -o test_sub_reg_2ndrun.sorted.out.ini
echo "Comparing second run and first run"
diff $flags test_sub_reg_1strun.sorted.out.ini test_sub_reg_2ndrun.sorted.out.ini

rm test_sub_reg.ini

echo Test passed -- no differences encountered.
exit 0
