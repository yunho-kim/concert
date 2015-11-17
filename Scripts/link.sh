#!/bin/bash

# check environment variables are correctly set
if [ -z ${SRCLIST} ]
then
  echo '$SRCLIST is not set. Please check that conf.sh is correctly set'
  exit 1
fi

if [ ! -r ${SRCLIST} ]
then
  echo "${SRCLIST} file is not found or not readable. Please check ${SRCLIST} is readable."
  exit 1
fi

if [ -z ${FUNCLIST} ]
then
  echo '$FUNCLIST is not set. Please check that conf.sh is correctly set'
  exit 1
fi

if [ ! -r ${FUNCLIST} ]
then
  echo "${FUNCLIST} file is not found or not readable. Please check ${FUNCLIST} is readable."
  exit 1
fi

if [ -z ${CRESTDIR} ]
then
  echo '$CRESTDIR is not set. Please check that conf.sh is correctly set'
  exit 1
fi

if [ ! -d ${CRESTDIR} ]
then
  echo "${CRESTDIR} directory does not exist."
  exit 1
fi

if [ ! -x ${CRESTDIR}/cil/bin/cilly ]
then
  echo "${CRESTDIR}/cil/bin/cilly does not exist or is not executable."
  exit 1
fi

if [ -z ${CONCERTDIR} ]
then 
  echo '$CONCERTDIR is not set. Please check that conf.sh is correctly set'
  exit 1
fi

if [ ! -x ${CONCERTDIR}/TestGenerator/MainGenerator ]
then
  echo "${CONCERTDIR}/TestGenerator/MainGenerator does not exist or is not executable."
  exit 1
fi

FILES=`cat ${SRCLIST}|xargs`
TARGET_FUNCS=`cat ${FUNCLIST}|xargs`
CC="${CRESTDIR}/cil/bin/cilly --save-temps --noPrintLn --doCrestInstrument -m32 -g -I${CRESTDIR}/include"

if [ ! -d objects ]
then
  echo "./objects directory is not found. Check the current working directory."
  exit 1
fi

for TARGET_FUNC in $TARGET_FUNCS
do
    echo "Linking for the $TARGET_FUNC function"
    mkdir -p test_"$TARGET_FUNC"
    cd objects
    cp initializer.dat idcount stmtcount funcount cfg_func_map cfg branches cfg_branches input -t ../test_"$TARGET_FUNC"
    cd ../test_"$TARGET_FUNC"

    ${CONCERTDIR}/TestGenerator/MainGenerator $TARGET_FUNC

    mv test_main.c test_"$TARGET_FUNC".c
    $CC -c test_"$TARGET_FUNC".c
    gcc -o test_$TARGET_FUNC test_"$TARGET_FUNC".o ../objects/tmp_*.o -lrt -lcrest -lyices -lstdc++ -L${CRESTDIR}/lib -g -m32
    cd ..
done
