#!/bin/bash

set -o pipefail

PATHS="/home/parkyongbae/gzip-1.5/*.c
/home/parkyongbae/gzip-1.5/lib/*.c
/home/parkyongbae/gzip-1.5/lib/glthread/*.c"

COUNT_TOTAL=0

rm -f assertCountResult.txt

for f in  $PATHS
do
	outputPath=${f//"gzip-1.5"/"gzip-1.5_mod"}
	commandline=$(/home/parkyongbae/AssertionInjector/AssertionInjector -o $outputPath $1 $f >> assertCountResult.txt)
	let "COUNT_TOTAL += 1"
done

echo "Total $COUNT_TOTAL files"
