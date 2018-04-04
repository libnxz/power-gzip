#

set title "p9 compress"
!grep COMP runs.txt | grep -v DECOM > runs.log
set term png
#
set logscale x 2
set format x "%L"
set xlabel "Input size, log_2(bytes)"
#
set format y "%g"
set ylabel "Throughput GB/s"
set output "compress.png"
plot 'runs.log' using 5:(($5/($12/$9))/1.0e9) with linespoints title 'p9 h/w compress', \
     'runs.log' using 5:(($5/(($12+$18)/$9))/1.0e9) with linespoints title '+page touch'

