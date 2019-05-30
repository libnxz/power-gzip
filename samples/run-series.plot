set terminal pngcairo size 900,600 enhanced font 'Verdana,12'
set output 'compress.png'
set logscale x 2 ; set format x '2^{%L}';
set ylabel 'Total compress throughput (GB/s)'
set xlabel 'Source data size (KB)'
set key right bottom
plot '<(grep "Total compress" log.log | grep "threads 1,")' using 7:5 title '1 thread' with linespoints,\
     '<(grep "Total compress" log.log | grep "threads 2,")' using 7:5 title '2 thread' with linespoints,\
     '<(grep "Total compress" log.log | grep "threads 4,")' using 7:5 title '4 thread' with linespoints,\
     '<(grep "Total compress" log.log | grep "threads 8,")' using 7:5 title '8 thread' with linespoints,\
     '<(grep "Total compress" log.log | grep "threads 16,")' using 7:5 title '16 thread' with linespoints,\
     '<(grep "Total compress" log.log | grep "threads 32,")' using 7:5 title '32 thread' with linespoints,\
     '<(grep "Total compress" log.log | grep "threads 64,")' using 7:5 title '64 thread' with linespoints,\
     '<(grep "Total compress" log.log | grep "threads 80,")' using 7:5 title '80 thread' with linespoints     

set output 'uncompress.png'
set logscale x 2 ; set format x '2^{%L}';
set ylabel 'Total uncompress throughput (GB/s)'
set xlabel 'Uncompressed data size (KB)'
set key right bottom
plot '<(grep "Total uncompress" log.log | grep "threads 1,")' using 7:5 title '1 thread' with linespoints,\
     '<(grep "Total uncompress" log.log | grep "threads 2,")' using 7:5 title '2 thread' with linespoints,\
     '<(grep "Total uncompress" log.log | grep "threads 4,")' using 7:5 title '4 thread' with linespoints,\
     '<(grep "Total uncompress" log.log | grep "threads 8,")' using 7:5 title '8 thread' with linespoints,\
     '<(grep "Total uncompress" log.log | grep "threads 16,")' using 7:5 title '16 thread' with linespoints,\
     '<(grep "Total uncompress" log.log | grep "threads 32,")' using 7:5 title '32 thread' with linespoints,\
     '<(grep "Total uncompress" log.log | grep "threads 64,")' using 7:5 title '64 thread' with linespoints,\
     '<(grep "Total uncompress" log.log | grep "threads 80,")' using 7:5 title '80 thread' with linespoints     
