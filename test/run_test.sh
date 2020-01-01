export NX_GZIP_CONFIG=./nx-zlib.conf
#export NX_GZIP_LOGFILE=./nx.log
export LD_LIBRARY_PATH=../:$LD_LIBRARY_PATH

./test_deflate
./test_inflate
./test_stress
./test_crc32
./test_adler32
