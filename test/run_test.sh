export NX_GZIP_LOGFILE=./nx.log
export LD_LIBRARY_PATH=../:$LD_LIBRARY_PATH

./test_deflate
./test_inflate

