#!/bin/bash
let "seed = 1"

for FSZ in `seq 8 30`
do
    echo "file size " $FSZ
    gcc -O3 -o makedata makedata.c -DLOGFILESZ=$FSZ

    for n in `seq 1 1000`
    do    

	for fname in `ls ./test/`
	do
	    echo "filename " ./test/$fname

	    let "++seed"
	    echo "seed" $seed
	    rm -f /mnt/ramdisk/jun*
	    ./makedata -s $seed < ./test/$fname > /mnt/ramdisk/junk
	    rc=$?; if [[ $rc != 0 ]]; then echo "EEEEEE makedata bad return code: filename size seed:" $fname, $FSZ, $seed; echo; echo; fi
	    ls -l /mnt/ramdisk/junk
	    
	    ./zpipe < /mnt/ramdisk/junk > /mnt/ramdisk/junk.z
	    rc=$?; if [[ $rc != 0 ]]; then echo "EEEEEE zpipe bad return code: filename size seed:" $fname, $FSZ, $seed; echo; echo; fi

	    ./zpipe -d < /mnt/ramdisk/junk.z > /mnt/ramdisk/junk.out
	    rc=$?; if [[ $rc != 0 ]]; then echo "EEEEEE zpipe -d bad return code: filename size seed:" $fname, $FSZ, $seed; echo; echo; fi
	    ls -l /mnt/ramdisk/junk.out
	    
	    cksum1=`cat /mnt/ramdisk/junk | sha1sum`
	    cksum2=`cat /mnt/ramdisk/junk.out | sha1sum`
	    echo $cksum1
	    echo $cksum2

	    if [[ $cksum1 != $cksum2 ]]; then
		echo "EEEEEE checksum mismatch"
	    fi

	    echo "AAAAAA"
	    echo "AAAAAA"	    
	    
	done
    done
done

 
