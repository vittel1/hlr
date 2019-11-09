#!/usr/bin/env gnuplot

set term pdf;
set termopt enhanced

in_file="laufzeiten_interlines.txt"

ymin = 0
ymax = 2740

set xrange [0 : 1024]
set yrange [ymin : ymax]

set ytics ymin,300,ymax

set xlabel "Anzahl Interlines"
set ylabel "Laufzeit in s"

set key left top

set output "interlines_plot.pdf"

plot in_file with linespoint lt rgb "#FF0000" title ""
