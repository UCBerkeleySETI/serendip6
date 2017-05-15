#!/bin/bash

VERS6SW=0.7.5                   \
VERS6GW=0.1.0                   \

# Add directory containing this script to PATH
PATH="$(dirname $0):${PATH}"

hostname=`hostname -s`
#net_thread=${1:-s6_pktsock_thread}
net_thread=${1:-s6_fake_net_thread}

mys6cn="blc00"


# Setup parameters for ONE instance.
#instance_i=("0" "1")
instance_i=("0")
log_timestamp=`date +%Y%m%d_%H%M%S`
instances=(
  # 2 x E5-2630 (6-cores @ 2.3 GHz, 15 MB L3, 7.2 GT/s QPI, 1333 MHz DRAM)
  # Save core  0 for OS.
  # Save core  5 for eth2 and eth3
  # Save core  6 for symmetry with core 0
  # Save core 11 for eth4 and eth5
  #
  # Setup for two GPU devices
  #
  #             CPUs
  #  masks   9876543210          
  # 0x000e = 0000001110, thus CPUs 1,2,3
  # 0x0380 = 1110000000, thus CPUs 7,8,9
  #
  #                               GPU NET GPU OUT
  # mask  bind_host               DEV CPU CPU CPU
  "0x000e ${hostname}-3.tenge.pvt  0   1   2   3 $log_timestamp" # Instance 0, eth3
);

function init() {
  instance=$1
  mask=$2
  bindhost=$3
  gpudev=$4
  netcpu=$5
  gpucpu=$6
  outcpu=$7
  log_timestamp=$8

  if [ -z "${mask}" ]
  then
    echo "Invalid instance number '${instance}' (ignored)"
    return 1
  fi

  if [ -z "$outcpu" ]
  then
    echo "Invalid configuration for host ${hostname} instance ${instance} (ignored)"
    return 1
  fi

  if [ $net_thread == 's6_pktsock_thread' ]
  then
    bindhost="eth$((3+2*instance))"
    echo "binding $net_thread to $bindhost"
  fi

  echo taskset $mask                   \
  hashpipe -p serendip6 -I $instance   \
    -o VERS6SW=$VERS6SW                \
    -o VERS6GW=$VERS6GW                \
    -o RUNALWYS=0                      \
    -o MAXHITS=2048                    \
    -o BINDHOST=$bindhost              \
    -o GPUDEV=$gpudev                  \
    -c $netcpu $net_thread             \
    -c $gpucpu s6_gpu_thread           \
    -c $outcpu s6_output_thread    

  taskset $mask                        \
  hashpipe -p serendip6 -I $instance   \
    -o VERS6SW=$VERS6SW                \
    -o VERS6GW=$VERS6GW                \
    -o RUNALWYS=0                      \
    -o MAXHITS=2048                    \
    -o BINDHOST=$bindhost              \
    -o GPUDEV=$gpudev                  \
    -c $netcpu $net_thread             \
    -c $gpucpu s6_gpu_thread           \
    -c $outcpu s6_output_thread        \
     < /dev/null                       \
    1> ${mys6cn}.out.$log_timestamp.$instance \
    2> ${mys6cn}.err.$log_timestamp.$instance &
}

# Start all instances
for instidx in ${instance_i[@]}
do
  args="${instances[$instidx]}"
  if [ -n "${args}" ]
  then
    echo
    echo Starting instance $mys6cn/$instidx
    init $instidx $args
    echo Instance $mys6cn/$instidx pid $!
    # Sleep to let instance come up
    sleep 10
  else
    echo Instance $instidx not defined for host $hostname
  fi
done

if [ $net_thread == 's6_pktsock_thread' ]
then
  # Zero out MISSEDPK counts
  for instidx in ${instance_i[@]}
  do
    for key in MISSEDPK NETDRPTL
    do
      echo Resetting $key count for $mys6cn/$instidx
      hashpipe_check_status -I $instidx -k $key -s 0
    done
  done
else
  # Zero out MISSPKTL counts
  for instidx in ${instance_i[@]}
  do
    echo Resetting MISSPKTL count for $mys6cn/$instidx
    hashpipe_check_status -I $instidx -k MISSPKTL -s 0
  done

  # Release NETHOLD
  for instidx in ${instance_i[@]}
  do
    echo Releasing NETHOLD for $mys6cn/$instidx
    hashpipe_check_status -I $instidx -k NETHOLD -s 0
  done

  # run always
  for instidx in ${instance_i[@]}
  do
    echo Turning on RUNALWYS for $mys6cn/$instidx
    hashpipe_check_status -I $instidx -k RUNALWYS -s 1
  done
fi

  # test mode
  for instidx in ${instance_i[@]}
  do
    echo Turning on TESTMODE for $mys6cn/$instidx
    hashpipe_check_status -I $instidx -k TESTMODE -s 1
  done
