#!/bin/bash

function start_evs()
{
	sleep 5
	proc=`ps -ef | grep iotjs | grep -v grep`
	if [ "$proc" == "" ]
	then
		workDir=$(cd $(dirname $0)/..; pwd)
		export PATH=$PATH:$workDir/bin
		export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$workDir/lib/
		export UV_THREADPOOL_SIZE=10
		export JS_LOG_FILE=$workDir/log/evs.log
		mkdir -p $workDir/log
		cd $workDir/smartSpeakerApp
		iotjs main.js $workDir/iFLYOS.json 5000 >$workDir/log/evs.log 2>&1 &
	#	iotjs main.js $workDir/iFLYOS.json 5000
	fi
}

function stop_evs()
{
	proc=`ps -ef | grep evsCtrl | grep -v grep | grep -v stop| awk '{print $2}'`
	if [ "$proc" != "" ]; then
    	kill -9 $proc
	fi

	proc=`ps -ef | grep iotjs | grep -v grep`
	if [ "$proc" != "" ]; then
    	killall -9 iotjs
	fi

	proc=`ps -ef | grep btgatt-server | grep -v grep`
	if [ "$proc" != "" ]; then
		killall -9 btgatt-server
	fi

	proc=`ps -ef | grep RMediaPlayer | grep -v grep`
	if [ "$proc" != "" ]; then
		killall -9 RMediaPlayer
	fi
}

case "$1" in
	start)
		start_evs
		;;
	stop)
	    stop_evs
		;;
	restart)
	    stop_evs
		start_evs
		;;
	*)
		;;
esac

exit $?
