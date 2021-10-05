export NX_GZIP_CONFIG=./nx-zlib.conf
#export NX_GZIP_LOGFILE=./nx.log
export LD_LIBRARY_PATH=../lib:$LD_LIBRARY_PATH

TS=$(date +"%F-%H-%M-%S")
run_test_log=run_test_${TS}.log
run_test_report=run_test_report_${TS}.txt
> $run_test_log
> $run_test_report

echo "running test_deflate..."
$TEST_WRAPPER ./test_deflate >> $run_test_log 2>&1
if [ $? -ne 0 ]; then
    echo "test_deflate failed."
    exit 1;
fi
echo "running test_inflate..."
$TEST_WRAPPER ./test_inflate >> $run_test_log 2>&1
if [ $? -ne 0 ]; then
    echo "test_inflate failed."
    exit 1;
fi
echo "running test_stress..."
$TEST_WRAPPER ./test_stress  >> $run_test_log 2>&1
echo "running test_crc32..."
$TEST_WRAPPER ./test_crc32   >> $run_test_log 2>&1
if [ $? -ne 0 ]; then
    echo "test_crc32 failed."
    exit 1;
fi
echo "running test_adler32..."
$TEST_WRAPPER ./test_adler32 >> $run_test_log 2>&1
if [ $? -ne 0 ]; then
    echo "test_adler32 failed."
    exit 1;
fi
echo "running test_multithread_stress..."
nthreads=${TEST_NTHREADS:-$(nproc)}
$TEST_WRAPPER ./test_multithread_stress $nthreads 10 6  >> $run_test_log 2>&1
if [ $? -ne 0 ]; then
    echo "test_multithread_stress failed."
    exit 1;
fi
echo "running test_pid_reuse..."
$TEST_WRAPPER ./test_pid_reuse 2 >> $run_test_log 2>&1
if [ $? -ne 0 ]; then
    echo "test_pid_reuse failed."
    exit 1;
fi
echo "------------------------------"

grep -E 'run_case|failed' $run_test_log | tee -a $run_test_report

grep -A 17 Thread $run_test_log | tee -a $run_test_report
