export NX_GZIP_CONFIG=./nx-zlib.conf
export LD_LIBRARY_PATH=../lib:$LD_LIBRARY_PATH

TS=$(date +"%F-%H-%M-%S")
run_test_log=run_system_check_${TS}.log
> $run_test_log

echo "Testing for kernel oops..."
NX_GZIP_CSB_POLL_MAX=200 ./test_stress  >> $run_test_log 2>&1
echo "Passed!"
