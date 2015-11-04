#!/bin/bash
GENERATOR=path/to/TestGenerator/Generator
SRCDIR=`pwd`/../src
FILES=""
CC="path/to/crest/cil/bin/cilly --doCrestInstrument -m32 -g -Ipath/to/crest/include"
export BRFUNCS="`pwd`/brfuncs.txt"
    mkdir -p objects
    cd objects
    rm -rf tmp_*

    for file in $FILES
    do
        $GENERATOR $SRCDIR/$file ALL_SS > ttmp_$file
        grep -v '^# ' ttmp_$file > tttmp_$file
        sed -e 's/struct __va_list_tag \[1\]/va_list/g' tttmp_$file > tmp_$file
        rm -rf ttmp_$file tttmp_$file

    done
    cd ../

