#!/bin/bash

# Add directory containing this script to PATH
PATH="$(dirname $0):${PATH}"

instance=$1
gen_fake=$2

shopt -s extglob
if [[ $instance != +([0-9]) ]]
then
    echo "s6_init.sh [0|1]"
    exit 1
fi

if [ $instance -eq 0 ]
then
    netcpu=1
    gpudev=0
elif [ $instance -eq 1 ]
then
    netcpu=7
    gpudev=1
else
    echo instance must be 0 or 1
    exit 1
fi

gpucpu=$(($netcpu + 1))
outcpu=$(($netcpu + 2))
bindhost=10.10.10.2

hashpipe -p serendip6 -I $instance  \
    -o BINDHOST=$bindhost           \
    -o VERS6SW=0.0.1                \
    -o VERS6GW=0.0.1                \
    -o GPUDEV=$gpudev               \
    -o RUNALWYS=1                   \
    -o MAXHITS=2048                 \
    -o GENFAKE=$gen_fake            \
    -c $netcpu s6_fake_net_thread   \
    -c $gpucpu s6_gpu_thread        \
    -c $outcpu s6_output_thread 

