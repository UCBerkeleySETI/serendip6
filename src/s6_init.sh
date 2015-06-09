#!/bin/bash

VERS6SW=0.6.0                   \
VERS6GW=0.1.0                   \

# Add directory containing this script to PATH
PATH="$(dirname $0):${PATH}"

hostname=`hostname -s`
net_thread=${1:-s6_pktsock_thread}

function getip() {
  out=$(host $1) && echo $out | awk '{print $NF}'
}

myip=$(getip $(hostname))

# Determine which, if any, s6cN alias maps to IP of current host.
# If a s6cN match is found, mys6cn gets set to N (i.e. just the numeric part).
# If no match is found, mys6cn will be empty.
mys6gbcn=
for s in {0..3}
do
  ip=$(getip s6gbc${s}.s6.pvt)
  [ "${myip}" == "${ip}" ] || continue
  mys6gbcn=$s
done

# If no s6cN alias maps to IP of current host, abort
if [ -z ${mys6gbcn} ]
then
  echo "$(hostname) is not aliased to a s6cN name"
  exit 1
fi

# Setup parameters for two instances.
instance_i=("0" "1")
#instance_i=("0")
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
  "0x0380 ${hostname}-5.tenge.pvt  1   7   8   9 $log_timestamp" # Instance 2, eth5
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
    1> s6gbc${mys6gbcn}.out.$log_timestamp.$instance \
    2> s6gbc${mys6gbcn}.err.$log_timestamp.$instance &
}

# Start all instances
for instidx in ${instance_i[@]}
do
  args="${instances[$instidx]}"
  if [ -n "${args}" ]
  then
    echo
    echo Starting instance s6gbc$mys6gbcn/$instidx
    init $instidx $args
    echo Instance s6gbc$mys6gbcn/$instidx pid $!
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
      echo Resetting $key count for s6gbc$mys6gbcn/$instidx
      hashpipe_check_status -I $instidx -k $key -s 0
    done
  done
else
  # Zero out MISSPKTL counts
  for instidx in ${instance_i[@]}
  do
    echo Resetting MISSPKTL count for s6gbc$mys6gbcn/$instidx
    hashpipe_check_status -I $instidx -k MISSPKTL -s 0
  done

  # Release NETHOLD
  for instidx in ${instance_i[@]}
  do
    echo Releasing NETHOLD for s6gbc$mys6gbcn/$instidx
    hashpipe_check_status -I $instidx -k NETHOLD -s 0
  done
fi
