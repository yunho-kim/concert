#!/bin/bash
FILES=""
TARGET_FUNCS=""
CC="path/to/crest/cil/bin/cilly --save-temps --noPrintLn --doCrestInstrument -m32 -g -Ipath/to/crest/include"

for TARGET_FUNC in $TARGET_FUNCS
do
    mkdir -p test_"$TARGET_FUNC"
    cd objects
    cp initializer.dat idcount stmtcount funcount cfg_func_map cfg branches cfg_branches input -t ../test_"$TARGET_FUNC"
    cd ../test_"$TARGET_FUNC"

    path/to/TestGenerator/MainGenerator $TARGET_FUNC

    mv test_main.c test_"$TARGET_FUNC".c
    $CC -c test_"$TARGET_FUNC".c
    gcc -o test_$TARGET_FUNC test_"$TARGET_FUNC".o ../objects/tmp_*.o -lrt -lcrest2 -lz3-gmp -lgmp -lstdc++ -L/home/yhkim/research/unit_testing/crest/lib -g -m32
    cd ..
done
