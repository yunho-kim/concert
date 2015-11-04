#!/bin/bash
CREST=path/to/crest/bin/run_crest
STRATEGY="dfs"
ITERS="100000"
TARGET_FUNCS=""
for $func in $TARGET_FUNCS
do
    cd test_$func
    $CREST "test_$func < /dev/null" $ITERS -$STRATEGY &> output_"test_$func"_"$ITERS"_"$STRATEGY".txt
    mv coverage coverage_"test_$func"_"$ITERS"_"$STRATEGY"
    cd ../
done
