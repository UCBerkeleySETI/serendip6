#! /bin/bash

BIN=/home/jeffc/bin

valon_time=`date +"%s"`

# The A side provides the sampling clock to the roach2's.
synth_A_freq=`$BIN/valon | grep "Synth A" | grep MHz | awk '{print $3}'`
synth_A_dBm=`$BIN/valon | grep "Synth A" | grep dBm | awk '{print $3}'`
synth_A_lock=`$BIN/valon | grep "Synth A" | grep Lock | awk '{print $6}'`
# A numeric "boolean" will be easier to deal with down stream than a string.
if [ $synth_A_lock == "True" ]
then
    synth_A_lock=1
else
    synth_A_lock=0
fi

# The B side provides the always-on birdie, injected at IF.
synth_B_freq=`$BIN/valon | grep "Synth B" | grep MHz | awk '{print $3}'`
synth_B_dBm=`$BIN/valon | grep "Synth B" | grep dBm | awk '{print $3}'`
synth_B_lock=`$BIN/valon | grep "Synth B" | grep Lock | awk '{print $6}'`
if [ $synth_B_lock == "True" ]
then
    synth_B_lock=1
else
    synth_B_lock=0
fi

redis-cli HMSET CLOCKSYN CLOCKTIM "$valon_time" CLOCKFRQ "$synth_A_freq" CLOCKDBM "$synth_A_dBm" CLOCKLOC "$synth_A_lock"
redis-cli HMSET BIRDISYN BIRDITIM "$valon_time" BIRDIFRQ "$synth_B_freq" BIRDIDBM "$synth_B_dBm" BIRDILOC "$synth_B_lock"
