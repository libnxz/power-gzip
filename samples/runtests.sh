#!/bin/bash

for fname in `ls ./test/`
do
    echo "filename " ./test/$fname
    for FSZ in `seq 8 30`
    do
	echo "file size " $FSZ
	gcc -O3 -o makedata makedata.c -DLOGFILESZ=$FSZ
	for seed in `seq 1 1000`
	do
	    rm -f /mnt/ramdisk/jun*
	    ./makedata -s $seed < ./test/$fname > /mnt/ramdisk/junk
	    rc=$?; if [[ $rc != 0 ]]; then echo "EEEEEE makedata bad return code: filename size seed:" $fname, $FSZ, $seed; echo; echo; fi
	    ls -l /mnt/ramdisk/junk
	    
	    ./zpipe < /mnt/ramdisk/junk > /mnt/ramdisk/junk.z
	    rc=$?; if [[ $rc != 0 ]]; then echo "EEEEEE zpipe bad return code: filename size seed:" $fname, $FSZ, $seed; echo; echo; fi

	    ./zpipe -d < /mnt/ramdisk/junk.z > /mnt/ramdisk/junk.out
	    rc=$?; if [[ $rc != 0 ]]; then echo "EEEEEE zpipe -d bad return code: filename size seed:" $fname, $FSZ, $seed; echo; echo; fi

	    sha1sum /mnt/ramdisk/junk /mnt/ramdisk/junk.out
	    
	done
    done
done

 
