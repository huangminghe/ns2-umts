reset
#set terminal post eps enhanced
set terminal emf
#set output "distance.eps"
set output "distance.emf"
set nogrid
#set logscale y
set xlabel "Going down ratio ({/symbol a})"
set ylabel "Distance from cell border (m)"
set xrange [1:1.7]
set yrange [0:10]
#set key 45,0.45
set title "Effect of link going down threshold on anticipation"
plot "result.t" using 10:9 title "" with lp
