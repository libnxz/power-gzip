POWER9 NX zlib compliant software (under construction)

## How to Use
- If want to use nxzlib to substitute zlib, following the steps as below:    
1. Build libnxz.so
```
make clean; make
```
2. Use libnxz.so to substitute libz.so    
```
cp libnxz.so /usr/lib/libnxz.so
mv /usr/lib/libz.so /usr/lib/libz.so.bak
ln -s /usr/lib/libnxz.so /usr/lib/libz.so
```
- If don't want to override the libz.so, use LD_PRELOAD to run. Something like:    
```
LD_PRELOAD=./libnxz.so /home/your_pragram
```
    
- If want to use nxzlib standalone, following the steps as below:
1. Edit Makefile and remove line "ZLIB = -DZLIB_API"
2. Build libnxz.so
```
make clean; make
```

## How to Run Test
1. Regression test:
```
cd test
make clean; make
./run_test.sh
```
2. NX multi-thread throughput test
```
cd samples
make clean; make
./run-series.sh /tmp/kernel.tar (note, you can change the input file)
(The output is saved in file log.log as input for run-series.plot)
```
3. z-testsuite

refer to: https://github.ibm.com/abali/power-gzip/issues/112

## How to Select NXs
By default, the NX-GZIP device with the nearest process to cpu affinity is selected
Consider using numactl -N 0 (or 8) to force your process attach to a particular device
~~If you want to use 1 NX or 2 NXs, use NX_GZIP_DEV_NUM variable.~~
~~1. Use "export NX_GZIP_DEV_NUM=-1" to use only one NX whose id is arbitrary.~~
~~2. Use "export NX_GZIP_DEV_NUM=0" to use only one NX whose id is 0.~~
~~3. Use "export NX_GZIP_DEV_NUM=1" to use only one NX whose id is 1.~~
~~4. Use "export NX_GZIP_DEV_NUM=2" to use two NXs.~~

## How to enable log and trace for debug
The default log will be /tmp/nx.log. Use "export NX_GZIP_LOGFILE=your.log" to specify a different log.  
By default, only errors will be recorded in log.  
Use "export NX_GZIP_VERBOSE=2" to record the more information.
Use "export NX_GZIP_TRACE=1" to enable logic trace.  
Use "export NX_GZIP_TRACE=8" to enable statistics trace.  

## Supported Functions List
Currently, supported the following functions.  
If want to use nxzlib standalone, add a prefix 'nx_' before the function.  
For example, use "nx_compress" instead of "compress".    

- compress, compress2, compressBound
- uncompress, uncompress2
     
- inflateInit_, inflateInit2_, inflateEnd, inflate
    
- deflateInit_, deflateInit2_, deflateEnd, deflate, deflateBound
