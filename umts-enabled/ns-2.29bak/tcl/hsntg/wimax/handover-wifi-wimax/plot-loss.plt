reset
#set terminal post eps enhanced
set terminal emf
#set output "loss-going-down.eps"
set output "loss-going-down.emf"
set nogrid
#set logscale y
set xlabel "Going down ratio ({/symbol a})"
set ylabel "Packet loss"
set xrange [1:1.7]
set yrange [0:140]
#set key 45,0.45
set title "Effect of link going down threshold on packet loss"
plot "result.t" using 10:8 title "" with lp
