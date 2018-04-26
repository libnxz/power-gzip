#!/bin/bash

#filename and number of instances
echo $1 $2

C=$2

rm -f *.nx.gunzip

for S in `seq 1 $C` 
do
    FN=$S.$1
	echo $FN
	rm -f $FN

	# create the unique file
    cp $1 $FN

	# FS cache warmup
    cat $FN > /dev/null
done

for S in `seq 1 $C`
do
	FN=$S.$1    
	if [ $(( $S % 2)) -eq 1 ]
	then
		numactl -N 0 ./gunzip_nx_test $FN  &
	else
		numactl -N 8 ./gunzip_nx_test $FN  &
	fi
done

wait


 
