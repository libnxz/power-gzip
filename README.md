POWER9 NX zlib compliant software (under construction)

## How to Use
- If want to use nxzlib to substitute zlib, following the steps as below:
1. Build libnxz.so
```
make clean; make
```
2. Use libnxz.so to substitute libz.so (replace 0.0 with the version being used)
```
cp lib/libnxz.so.0.0 /usr/lib/
mv /usr/lib/libz.so /usr/lib/libz.so.bak
ln -s /usr/lib/libnxz.so.0.0 /usr/lib/libz.so
```
- If don't want to override the libz.so, use LD_PRELOAD to run. Something like:
```
LD_PRELOAD=./libnxz.so /home/your_pragram
```

- If want to use nxzlib standalone, following the steps as below:
1. Edit `config.mk` and remove line "ZLIB = -DZLIB_API"
2. Build libnxz.so
```
make clean; make
```

## How to Run Test
```
cd test
make clean; make
make check
```

## How to Select NXs
By default, the NX-GZIP device with the nearest process to cpu affinity is
selected. Consider using numactl -N 0 (or 8) to force your process attach to a
particular device

## How to enable log and trace for debug
The default log will be /tmp/nx.log. Use `export NX_GZIP_LOGFILE=your.log`
to specify a different log. By default, only errors will be recorded in log.

Use `export NX_GZIP_VERBOSE=2` to record the more information.

Use `export NX_GZIP_TRACE=1` to enable logic trace.

Use `export NX_GZIP_TRACE=8` to enable statistics trace.

## Supported Functions List
Currently, supported the following functions.
If want to use nxzlib standalone, add a prefix 'nx_' before the function.
For example, use "nx_compress" instead of "compress".

- compress, compress2, compressBound
- uncompress, uncompress2
- inflateInit_, inflateInit2_, inflateEnd, inflate
- deflateInit_, deflateInit2_, deflateEnd, deflate, deflateBound
