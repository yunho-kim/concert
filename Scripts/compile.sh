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


if [ ! -d objects ]
then
  echo "./objects directory is not found. Check the current working directory."
  exit 1
fi

FILES=`cat ${SRCLIST}|xargs`
CC="${CRESTDIR}/cil/bin/cilly --doCrestInstrument --save-temps --noPrintLn --noWrap -m32 -g -I${CRESTDIR}/include"

cd objects
rm -rf *.o initializer.dat idcount stmtcount funcount cfg_func_map cfg branches cfg_branches input

for file in $FILES
do
    $CC -c -m32 tmp_$file
done
cd ../
