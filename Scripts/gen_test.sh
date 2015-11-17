#!/bin/bash

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

if [ -z ${CONCERTDIR} ]
then
  echo '$CONCERTDIR is not set. Please check that conf.sh is correctly set'
  exit 1
fi

if [ ! -x ${CONCERTDIR}/TestGenerator/Generator ]
then
  echo "${CONCERTDIR}/TestGenerator/Generator does not exist or is not executable."
  exit 1
fi

GENERATOR=${CONCERTDIR}/TestGenerator/Generator
FILES=`cat ${SRCLIST}|xargs`
CC="${CRESTDIR}/cil/bin/cilly --doCrestInstrument -m32 -g -Ipath/to/crest/include"

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

