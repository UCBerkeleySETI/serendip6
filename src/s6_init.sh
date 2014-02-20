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
    netcpu=2
    gpudev=0
elif [ $instance -eq 1 ]
then
    netcpu=12
    gpudev=1
else
    echo instance must be 0 or 1
    exit 1
fi

gpucpu=$(($netcpu + 2))
outcpu=$(($netcpu + 4))

hashpipe -p serendip6 -I $instance  \
    -o VERS6SW=0.0.1                \
    -o VERS6GW=0.0.1                \
    -o GPUDEV=$gpudev               \
    -o RUNALWYS=1                   \
    -o GENFAKE=$gen_fake            \
    -o MAXHITS=2048                 \
    -c $netcpu s6_fake_net_thread   \
    -c $gpucpu s6_gpu_thread        \
    -c $outcpu s6_output_thread 
#    -o BINDHOST=$bindhost \  10g IP

