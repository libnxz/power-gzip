#!/bin/bash

plotfn=log_2nx.log

nodes=`numastat | head -1`
for th in 1 8 32 64 80
do
    for a in `seq 0 20`  # size
    do
	b=$((1 << $a))
	nbyte=$(($b * 1024))
	rpt=$((1000 * 1000 * 1000 * 10)) # 10GB
	rpt=$(( ($rpt+$nbyte-1)/$nbyte )) # iters
	rpt=$(( ($rpt+$th-1)/$th )) # per thread
	rm -f junk2
	head -c $nbyte $1 > junk2;
	ls -l junk2;
	for i in $nodes
	do
		node=`echo $i | cut -c 5`
		numactl -N $node ./compdecomp_th junk2 $th $rpt &
	done
	wait
    done
done  > $plotfn 2>&1


