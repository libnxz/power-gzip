#!/bin/bash

plotfn=log_2nx.log

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
	numactl -N 0 ./compdecomp_th junk2 $th $rpt &
	numactl -N 8 ./compdecomp_th junk2 $th $rpt &
	wait
    done
done  > $plotfn 2>&1


