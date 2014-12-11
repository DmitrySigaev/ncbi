#!/bin/bash

function create_output_file()
{
   local args=$1
   local result=$2

   (cd test-cases && echo "$TEST_TOOL_PATH $args -o - "| bash  \
    > $result 2>&1 ) || echo "$TEST_TOOL_PATH" "returned non-zero exit code"
}

function make_test_name()
{
   echo "$1" | sed -r -e 's/^-i //' -e 's/[ &=\\.\\(\\),\:\/"]+/-/g' -e 's/--/-/g'
}
