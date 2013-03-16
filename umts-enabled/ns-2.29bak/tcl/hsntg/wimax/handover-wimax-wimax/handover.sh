#!/bin/bash

#global variables
NB_RUN=10

#execute the run for a scpecific DCD
run_link_down ()
{
    echo "running down"
    RUN=0
    L2_HANDOFF=0
    L3_HANDOFF=0
    TOTAL_HANDOFF=0
    LOST=0
    SEED=1
    while [ $RUN -lt $NB_RUN ]; do
	ns ../handover.tcl $SEED 5 0 0 &>log.t
	let SEED=SEED+1
	#parse results
	START=`grep DOWN log.t | awk '{print $2}'`
	START=`echo $START | awk '{print $1-0.6}'`
	#echo $START
	STOP_ALL=`grep "received ack" log.t`
	STOP=`echo $STOP_ALL | awk '{print $15}'`
	STOP_L2_ALL=`grep "LINK UP" log.t`
	STOP_L2=`echo $STOP_L2_ALL | awk '{print $12}'`
	#echo $STOP	
	TOTAL_HANDOFF=`echo $TOTAL_HANDOFF $STOP $START | awk '{print $1+$2-$3}'`
	L2_HANDOFF=`echo $L2_HANDOFF $STOP_L2 $START | awk '{print $1+$2-$3}'`
	#echo $TOTAL_HANDOFF $STOP

	#packet loss
	RECV=`grep "cbr" out.res | grep ^r | grep "AGT" | awk 'BEGIN{i=0}{i++}END{print i}'`
        SEND=`grep "0 1 cbr" out.res | grep ^+ | awk 'BEGIN{i=0}{i++}END{print i}'`
	LOST=`echo $LOST $RECV $SEND | awk '{print $1+$3-$2}'`
	TMP=`echo $RECV $SEND | awk '{print $2-$1}'`
	echo "Run "$RUN " start=" $START " stop=" $STOP " lost=" $TMP
	let RUN=RUN+1
    done
    AVERAGE_HANDOFF=`echo $TOTAL_HANDOFF $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L2=`echo $L2_HANDOFF $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L3=`echo $AVERAGE_HANDOFF $AVERAGE_HANDOFF_L2 | awk '{print $1-$2}'`
    AVERAGE_LOST=`echo $LOST $NB_RUN | awk '{print $1/$2}'`
    echo "down handoff =" $AVERAGE_HANDOFF "L2handoff=" $AVERAGE_HANDOFF_L2 "L3handoff=" $AVERAGE_HANDOFF_L3 "lost=" $AVERAGE_LOST
    echo "down average handoff link down=" $AVERAGE_HANDOFF "L2handoff=" $AVERAGE_HANDOFF_L2 "L3handoff=" $AVERAGE_HANDOFF_L3 "lost=" $AVERAGE_LOST >>../result.t
}

run_link_going_down1 ()
{
    echo "running going down 1"
    RUN=0
    SEED=1
    L2_HANDOFF=0
    L3_HANDOFF=0
    TOTAL_HANDOFF=0
    LOST=0
    while [ $RUN -lt $NB_RUN ]; do
	ns ../handover.tcl $SEED 5 1 0 &>log.t

	#parse results
	START=`grep imminent log.t | awk '{print $2}'`
	#echo $START
	while [ "$START" == "" ] ; do
	    echo "scanning not found..rerun"
	    let SEED=SEED+1
	    ns ../handover.tcl $SEED 5 1 0 &>log.t
	    START=`grep proceeding log.t | awk '{print $2}'`
	done
	let SEED=SEED+1
	STOP_ALL=`grep "received ack" log.t`
	STOP=`echo $STOP_ALL | awk '{print $15}'`
	STOP_L2_ALL=`grep "LINK UP" log.t`
	STOP_L2=`echo $STOP_L2_ALL | awk '{print $12}'`
	#echo $STOP	
	TOTAL_HANDOFF=`echo $TOTAL_HANDOFF $STOP $START | awk '{print $1+$2-$3}'`
	L2_HANDOFF=`echo $L2_HANDOFF $STOP_L2 $START | awk '{print $1+$2-$3}'`
	#echo $TOTAL_HANDOFF $STOP

	#packet loss
	RECV=`grep "cbr" out.res | grep ^r | grep "AGT" | awk 'BEGIN{i=0}{i++}END{print i}'`
        SEND=`grep "0 1 cbr" out.res | grep ^+ | awk 'BEGIN{i=0}{i++}END{print i}'`
	LOST=`echo $LOST $RECV $SEND | awk '{print $1+$3-$2}'`
	TMP=`echo $RECV $SEND | awk '{print $2-$1}'`
	echo "Run "$RUN " start=" $START " stop=" $STOP " lost=" $TMP
	let RUN=RUN+1
    done
    AVERAGE_HANDOFF=`echo $TOTAL_HANDOFF $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L2=`echo $L2_HANDOFF $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L3=`echo $AVERAGE_HANDOFF $AVERAGE_HANDOFF_L2 | awk '{print $1-$2}'`
    AVERAGE_LOST=`echo $LOST $NB_RUN | awk '{print $1/$2}'`
    echo "goingdown1 handoff =" $AVERAGE_HANDOFF "L2handoff=" $AVERAGE_HANDOFF_L2 "L3handoff=" $AVERAGE_HANDOFF_L3 "lost=" $AVERAGE_LOST
    echo "goingdown1 average handoff link down=" $AVERAGE_HANDOFF "L2handoff=" $AVERAGE_HANDOFF_L2 "L3handoff=" $AVERAGE_HANDOFF_L3 "lost=" $AVERAGE_LOST >>../result.t
}

run_link_going_down2 ()
{
    echo "running going down 2"
    RUN=0
    L2_HANDOFF=0
    L3_HANDOFF=0
    TOTAL_HANDOFF=0
    LOST=0
    SEED=1
    while [ $RUN -lt $NB_RUN ]; do
	ns ../handover.tcl $SEED 5 1 1 &>log.t

	#parse results
	START=`grep imminent log.t | awk '{print $2}'`
	#echo $START
	while [ "$START" == "" ] ; do
	    echo "scanning not found..rerun"
	    let SEED=SEED+1
	    ns ../handover.tcl $SEED 5 1 1 &>log.t
	    START=`grep proceeding log.t | awk '{print $2}'`
	done
	let SEED=SEED+1
	STOP_ALL=`grep "received ack" log.t`
	STOP=`echo $STOP_ALL | awk '{print $15}'`
	STOP_L2_ALL=`grep "LINK UP" log.t`
	STOP_L2=`echo $STOP_L2_ALL | awk '{print $12}'`
	#echo $STOP	
	TOTAL_HANDOFF=`echo $TOTAL_HANDOFF $STOP $START | awk '{print $1+$2-$3}'`
	L2_HANDOFF=`echo $L2_HANDOFF $STOP_L2 $START | awk '{print $1+$2-$3}'`
	#echo $TOTAL_HANDOFF $STOP

	#packet loss
	RECV=`grep "cbr" out.res | grep ^r | grep "AGT" | awk 'BEGIN{i=0}{i++}END{print i}'`
        SEND=`grep "0 1 cbr" out.res | grep ^+ | awk 'BEGIN{i=0}{i++}END{print i}'`
	LOST=`echo $LOST $RECV $SEND | awk '{print $1+$3-$2}'`
	TMP=`echo $RECV $SEND | awk '{print $2-$1}'`
	echo "Run "$RUN " start=" $START " stop=" $STOP " lost=" $TMP
	let RUN=RUN+1
    done
    AVERAGE_HANDOFF=`echo $TOTAL_HANDOFF $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_LOST=`echo $LOST $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L2=`echo $L2_HANDOFF $NB_RUN | awk '{print $1/$2}'`
    AVERAGE_HANDOFF_L3=`echo $AVERAGE_HANDOFF $AVERAGE_HANDOFF_L2 | awk '{print $1-$2}'`
    echo "goingdown2 handoff =" $AVERAGE_HANDOFF "L2handoff=" $AVERAGE_HANDOFF_L2 "L3handoff=" $AVERAGE_HANDOFF_L3 "lost=" $AVERAGE_LOST
    echo "goingdown2 average handoff link down=" $AVERAGE_HANDOFF "L2handoff=" $AVERAGE_HANDOFF_L2 "L3handoff=" $AVERAGE_HANDOFF_L3 "lost=" $AVERAGE_LOST >>../result.t
}


#main program
mkdir link_down
cd link_down
run_link_down
cd ..

mkdir link_going_down1
cd link_going_down1
run_link_going_down1
cd ..

mkdir link_going_down2
cd link_going_down2
run_link_going_down2
cd ..
