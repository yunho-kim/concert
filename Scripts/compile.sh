#!/bin/bash
FILES=""
CC="path/to/crest/crest/cil/bin/cilly --doCrestInstrument --save-temps --noPrintLn --noWrap -m32 -g -Ipath/to/crest/include"

rm -rf *.o initializer.dat idcount stmtcount funcount cfg_func_map cfg branches cfg_branches input

    for file in $FILES
    do
        $CC -c -m32 tmp_$file
    done
