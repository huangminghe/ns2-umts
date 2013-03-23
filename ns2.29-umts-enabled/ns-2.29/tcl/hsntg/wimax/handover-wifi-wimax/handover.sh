#!/bin/bash

#global variables
NB_RUN=10

run_link_down_iface_on ()
{
    echo "run_link_down_iface_on"
    RUN=0
    L2_HANDOFF_A=0
    L3_HANDOFF_A=0
    TOTAL_HANDOFF_A=0
    L2_HANDOFF_B=0
    L3_HANDOFF_B=0
    TOTAL_HANDOFF_B=0
    LOST=0
    SEED=1
    while [ $RUN -lt $NB_RUN ]; do
	ns ../wifi_wimax.tcl $SEED 1.0 0 &>log.t
	let SEED=SEED+1
	#parse results
	TMP=`grep DETECTED log.t`
	START_A=`echo $TMP | awk '{print $12}'`
	TMP=`grep DOWN log.t`
	START_B=`echo $TMP | awk '{print $2}'`
	TMP=`grep "LINK UP" log.t`
	STOP_L2_A=`echo $TMP | awk '{print $12}'`
	STOP_L2_B=$START_B
	TMP=`grep "received ack" log.t`
	STOP_A=`echo $TMP | awk '{print $14}'`
	STOP_B=`echo $TMP | awk '{print $26}'`
	#echo $STOP	
	TOTAL_HANDOFF_A=`echo $TOTAL_HANDOFF_A $STOP_A $START_A | awk '{print $1+$2-$3}'`
	L2_HANDOFF_A=`echo $L2_HANDOFF_A $STOP_L2_A $START_A | awk '{print $1+$2-$3}'`
	TOTAL_HANDOFF_B=`echo $TOTAL_HANDOFF_B $STOP_B $START_B | awk '{print $1+$2-$3}'`
	L2_HANDOFF_B=`echo $L2_HANDOFF_B $STOP_L2_B $START_B | awk '{print $1+$2-$3}'`
	#echo $TOTAL_HANDOFF $STOP

	#packet loss
	RECV=`grep "cbr" out.res | grep ^r | grep "MAC" | grep "Hs 6" | awk 'BEGIN{i=0}{i++}END{print i}'`
	RECV2=`grep "cbr" out.res | grep ^r | grep "MAC" | grep "Hs 4" | awk 'BEGIN{i=0}{i++}END{print i}'`
	let RECV=RECV+RECV2
        SEND=`grep "0 1 cbr" out.res | grep ^+ | awk 'BEGIN{i=0}{i++}END{print i}'`
	LOST=`echo $LOST $RECV $SEND | awk '{print $1+$3-$2}'`
	TMP=`echo $RECV $SEND | awk '{print $2-$1}'`
	echo "Run "$RUN " start=" $START_A " stop=" $STOP_A "start=" $START_B " stop=" $STOP_B " lost=" $TMP
	let RUN=RUN+1
    done
    AVERAGE_HANDOFF_A=`echo $TOTAL_HANDOFF_A $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L2_A=`echo $L2_HANDOFF_A $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L3_A=`echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A | awk '{print $1-$2}'`
    AVERAGE_HANDOFF_B=`echo $TOTAL_HANDOFF_B $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L2_B=`echo $L2_HANDOFF_B $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L3_B=`echo $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B | awk '{print $1-$2}'`
    AVERAGE_LOST=`echo $LOST $NB_RUN | awk '{print $1/$2}'`
    echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A $AVERAGE_HANDOFF_L3_A $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B $AVERAGE_HANDOFF_L3_B $AVERAGE_LOST
    echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A $AVERAGE_HANDOFF_L3_A $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B $AVERAGE_HANDOFF_L3_B $AVERAGE_LOST >>../result.t
}


run_link_down_iface_off ()
{
    echo "run_link_down_iface_off"
    RUN=0
    L2_HANDOFF_A=0
    L3_HANDOFF_A=0
    TOTAL_HANDOFF_A=0
    L2_HANDOFF_B=0
    L3_HANDOFF_B=0
    TOTAL_HANDOFF_B=0
    LOST=0
    SEED=1
    while [ $RUN -lt $NB_RUN ]; do
#	ns ../wifi_wimax.tcl $SEED 1.0 1 &>log.t
	ns ../wifi_wimax.tcl 6 1.0 1 &>log.t
	let SEED=SEED+1
	#parse results
	TMP=`grep DETECTED log.t`
	START_A=`echo $TMP | awk '{print $12}'`
	TMP=`grep DOWN log.t`
	START_B=`echo $TMP | awk '{print $12}'`
	TMP=`grep "LINK UP" log.t`
	STOP_L2_A=`echo $TMP | awk '{print $12}'`
	STOP_L2_B=`echo $TMP | awk '{print $22}'`
	TMP=`grep "received ack" log.t`
	STOP_A=`echo $TMP | awk '{print $14}'`
	STOP_B=`echo $TMP | awk '{print $26}'`
	#echo $STOP	
	TOTAL_HANDOFF_A=`echo $TOTAL_HANDOFF_A $STOP_A $START_A | awk '{print $1+$2-$3}'`
	L2_HANDOFF_A=`echo $L2_HANDOFF_A $STOP_L2_A $START_A | awk '{print $1+$2-$3}'`
	TOTAL_HANDOFF_B=`echo $TOTAL_HANDOFF_B $STOP_B $START_B | awk '{print $1+$2-$3}'`
	L2_HANDOFF_B=`echo $L2_HANDOFF_B $STOP_L2_B $START_B | awk '{print $1+$2-$3}'`
	#echo $TOTAL_HANDOFF $STOP

	#packet loss
	RECV=`grep "cbr" out.res | grep ^r | grep "MAC" | grep "Hs 6" | awk 'BEGIN{i=0}{i++}END{print i}'`
	RECV2=`grep "cbr" out.res | grep ^r | grep "MAC" | grep "Hs 4" | awk 'BEGIN{i=0}{i++}END{print i}'`
	let RECV=RECV+RECV2
        SEND=`grep "0 1 cbr" out.res | grep ^+ | awk 'BEGIN{i=0}{i++}END{print i}'`
	LOST=`echo $LOST $RECV $SEND | awk '{print $1+$3-$2}'`
	TMP=`echo $RECV $SEND | awk '{print $2-$1}'`
	echo "Run "$RUN " start=" $START_A " stop=" $STOP_A "start=" $START_B " stop=" $STOP_B " lost=" $TMP
	let RUN=RUN+1
    done
    AVERAGE_HANDOFF_A=`echo $TOTAL_HANDOFF_A $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L2_A=`echo $L2_HANDOFF_A $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L3_A=`echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A | awk '{print $1-$2}'`
    AVERAGE_HANDOFF_B=`echo $TOTAL_HANDOFF_B $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L2_B=`echo $L2_HANDOFF_B $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L3_B=`echo $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B | awk '{print $1-$2}'`
    AVERAGE_LOST=`echo $LOST $NB_RUN | awk '{print $1/$2}'`
    echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A $AVERAGE_HANDOFF_L3_A $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B $AVERAGE_HANDOFF_L3_B $AVERAGE_LOST
    echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A $AVERAGE_HANDOFF_L3_A $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B $AVERAGE_HANDOFF_L3_B $AVERAGE_LOST >>../result.t
}

run_link_going_down_iface_on ()
{
    echo "run_link_going_down_iface_on"
    RUN=0
    L2_HANDOFF_A=0
    L3_HANDOFF_A=0
    TOTAL_HANDOFF_A=0
    L2_HANDOFF_B=0
    L3_HANDOFF_B=0
    TOTAL_HANDOFF_B=0
    LOST=0
    SEED=1
    while [ $RUN -lt $NB_RUN ]; do
	ns ../wifi_wimax.tcl $SEED 1.1 0 &>log.t
	let SEED=SEED+1
	#parse results
	TMP=`grep DETECTED log.t`
	START_A=`echo $TMP | awk '{print $12}'`
	TMP=`grep "LINK GOING DOWN" log.t`
	START_B=`echo $TMP | awk '{print $2}'`
	TMP=`grep "LINK UP" log.t`
	STOP_L2_A=`echo $TMP | awk '{print $12}'`
	STOP_L2_B=$START_B
	TMP=`grep "received ack" log.t`
	STOP_A=`echo $TMP | awk '{print $14}'`
	STOP_B=`echo $TMP | awk '{print $26}'`
	#echo $STOP	
	TOTAL_HANDOFF_A=`echo $TOTAL_HANDOFF_A $STOP_A $START_A | awk '{print $1+$2-$3}'`
	L2_HANDOFF_A=`echo $L2_HANDOFF_A $STOP_L2_A $START_A | awk '{print $1+$2-$3}'`
	TOTAL_HANDOFF_B=`echo $TOTAL_HANDOFF_B $STOP_B $START_B | awk '{print $1+$2-$3}'`
	L2_HANDOFF_B=`echo $L2_HANDOFF_B $STOP_L2_B $START_B | awk '{print $1+$2-$3}'`
	#echo $TOTAL_HANDOFF $STOP

	#packet loss
	RECV=`grep "cbr" out.res | grep ^r | grep "MAC" | grep "Hs 6" | awk 'BEGIN{i=0}{i++}END{print i}'`
	RECV2=`grep "cbr" out.res | grep ^r | grep "MAC" | grep "Hs 4" | awk 'BEGIN{i=0}{i++}END{print i}'`
	let RECV=RECV+RECV2
        SEND=`grep "0 1 cbr" out.res | grep ^+ | awk 'BEGIN{i=0}{i++}END{print i}'`
	LOST=`echo $LOST $RECV $SEND | awk '{print $1+$3-$2}'`
	TMP=`echo $RECV $SEND | awk '{print $2-$1}'`
	echo "Run "$RUN " start=" $START_A " stop=" $STOP_A "start=" $START_B " stop=" $STOP_B " lost=" $TMP
	let RUN=RUN+1
    done
    AVERAGE_HANDOFF_A=`echo $TOTAL_HANDOFF_A $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L2_A=`echo $L2_HANDOFF_A $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L3_A=`echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A | awk '{print $1-$2}'`
    AVERAGE_HANDOFF_B=`echo $TOTAL_HANDOFF_B $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L2_B=`echo $L2_HANDOFF_B $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L3_B=`echo $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B | awk '{print $1-$2}'`
    AVERAGE_LOST=`echo $LOST $NB_RUN | awk '{print $1/$2}'`
    echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A $AVERAGE_HANDOFF_L3_A $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B $AVERAGE_HANDOFF_L3_B $AVERAGE_LOST
    echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A $AVERAGE_HANDOFF_L3_A $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B $AVERAGE_HANDOFF_L3_B $AVERAGE_LOST >>../result.t
}

run_link_going_down_iface_off ()
{
    echo "run_link_going_down_iface_off $1"
    RUN=0
    L2_HANDOFF_A=0
    L3_HANDOFF_A=0
    TOTAL_HANDOFF_A=0
    L2_HANDOFF_B=0
    L3_HANDOFF_B=0
    TOTAL_HANDOFF_B=0
    TOTAL_DISTANCE=0
    LOST=0
    SEED=1
    while [ $RUN -lt $NB_RUN ]; do
	ns ../wifi_wimax.tcl $SEED $1 1 &>log.t
	let SEED=SEED+1
	#parse results
	TMP=`grep DETECTED log.t`
	START_A=`echo $TMP | awk '{print $12}'`
	TMP=`grep DOWN log.t`
	START_B=`echo $TMP | awk '{print $12}'`
	TMP=`grep "LINK UP" log.t`
	STOP_L2_A=`echo $TMP | awk '{print $12}'`
	STOP_L2_B=`echo $TMP | awk '{print $22}'`
	TMP=`grep "received ack" log.t`
	STOP_A=`echo $TMP | awk '{print $14}'`
	STOP_B=`echo $TMP | awk '{print $26}'`
	TMP=`grep DOWN log.t | awk 'BEGIN{}{i=$2}END{print i}'`
	TOTAL_DISTANCE=`echo $TOTAL_DISTANCE $TMP $START_B | awk '{print $1+$2-$3}'`
	#echo $STOP	
	TOTAL_HANDOFF_A=`echo $TOTAL_HANDOFF_A $STOP_A $START_A | awk '{print $1+$2-$3}'`
	L2_HANDOFF_A=`echo $L2_HANDOFF_A $STOP_L2_A $START_A | awk '{print $1+$2-$3}'`
	TOTAL_HANDOFF_B=`echo $TOTAL_HANDOFF_B $STOP_B $START_B | awk '{print $1+$2-$3}'`
	L2_HANDOFF_B=`echo $L2_HANDOFF_B $STOP_L2_B $START_B | awk '{print $1+$2-$3}'`
	#echo $TOTAL_HANDOFF $STOP

	#packet loss
	RECV=`grep "cbr" out.res | grep ^r | grep "MAC" | grep "Hs 6" | awk 'BEGIN{i=0}{i++}END{print i}'`
	RECV2=`grep "cbr" out.res | grep ^r | grep "MAC" | grep "Hs 4" | awk 'BEGIN{i=0}{i++}END{print i}'`
	let RECV=RECV+RECV2
        SEND=`grep "0 1 cbr" out.res | grep ^+ | awk 'BEGIN{i=0}{i++}END{print i}'`
	LOST=`echo $LOST $RECV $SEND | awk '{print $1+$3-$2}'`
	TMP=`echo $RECV $SEND | awk '{print $2-$1}'`
	echo "Run "$RUN " start=" $START_A " stop=" $STOP_A "start=" $START_B " stop=" $STOP_B " lost=" $TMP
	let RUN=RUN+1
    done
    AVERAGE_HANDOFF_A=`echo $TOTAL_HANDOFF_A $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L2_A=`echo $L2_HANDOFF_A $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L3_A=`echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A | awk '{print $1-$2}'`
    AVERAGE_HANDOFF_B=`echo $TOTAL_HANDOFF_B $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L2_B=`echo $L2_HANDOFF_B $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L3_B=`echo $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B | awk '{print $1-$2}'`
    AVERAGE_LOST=`echo $LOST $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_DISTANCE=`echo $TOTAL_DISTANCE $NB_RUN | awk '{print $1/$2}'`
    echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A $AVERAGE_HANDOFF_L3_A $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B $AVERAGE_HANDOFF_L3_B $AVERAGE_LOST $AVERAGE_DISTANCE $1
    echo $AVERAGE_HANDOFF_A $AVERAGE_HANDOFF_L2_A $AVERAGE_HANDOFF_L3_A $AVERAGE_HANDOFF_B $AVERAGE_HANDOFF_L2_B $AVERAGE_HANDOFF_L3_B $AVERAGE_LOST $AVERAGE_DISTANCE $1 >>../result.t
}

#main program

mkdir link_down_iface_on
cd link_down_iface_on
run_link_down_iface_on
cd ..

mkdir link_down_iface_off
cd link_down_iface_off
run_link_down_iface_off
cd ..

mkdir link_going_down_iface_on
cd link_going_down_iface_on
run_link_going_down_iface_on
cd ..

for th in 1.01 1.05 1.1 1.15 1.2 1.3 1.4 1.5 ; do
    mkdir link_going_down_iface_off_$th
    cd link_going_down_iface_off_$th
    run_link_going_down_iface_off $th
    cd ..
done
