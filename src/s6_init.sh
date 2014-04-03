#!/bin/bash

# Add directory containing this script to PATH
PATH="$(dirname $0):${PATH}"

hostname=`hostname -s`

function getip() {
  out=$(host $1) && echo $out | awk '{print $NF}'
}

myip=$(getip $(hostname))

# Determine which, if any, s6cN alias maps to IP of current host.
# If a s6cN match is found, mys6cn gets set to N (i.e. just the numeric part).
# If no match is found, mys6cn will be empty.
mys6cn=
for s in {0..3}
do
  ip=$(getip s6c${s}.s6.pvt)
  [ "${myip}" == "${ip}" ] || continue
  mys6cn=$s
done

# If no s6cN alias maps to IP of current host, abort
if [ -z ${mys6cn} ]
then
  echo "$(hostname) is not aliased to a s6cN name"
  exit 1
fi

# Setup parameters for two instances.
instance_i=("0" "1");
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
  "0x000e ${hostname}-2.tenge.pvt  0   1   2   3" # Instance 0, eth2
  "0x0380 ${hostname}-4.tenge.pvt  1   7   8   9" # Instance 2, eth4
);

function init() {
  instance=$1
  mask=$2
  bindhost=$3
  gpudev=$4
  netcpu=$5
  gpucpu=$6
  outcpu=$7

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

  echo taskset $mask                   \
  hashpipe -p serendip6 -I $instance   \
    -o VERS6SW=0.0.1                   \
    -o VERS6GW=0.0.1                   \
    -o RUNALWYS=0                      \
    -o MAXHITS=2048                    \
    -o BINDHOST=$bindhost              \
    -o GPUDEV=$gpudev                  \
    -c $netcpu s6_net_thread           \
    -c $gpucpu s6_gpu_thread           \
    -c $outcpu s6_output_thread    

  taskset $mask                        \
  hashpipe -p serendip6 -I $instance   \
    -o VERS6SW=0.0.1                   \
    -o VERS6GW=0.0.1                   \
    -o RUNALWYS=0                      \
    -o MAXHITS=2048                    \
    -o BINDHOST=$bindhost              \
    -o GPUDEV=$gpudev                  \
    -c $netcpu s6_net_thread           \
    -c $gpucpu s6_gpu_thread           \
    -c $outcpu s6_output_thread        \
     < /dev/null                       \
    1> s6c${mys6cn}.out.$instance      \
    2> s6c${mys6cn}.err.$instance &
}

# Start all instances
for instidx in ${instance_i[@]}
do
  args="${instances[$instidx]}"
  if [ -n "${args}" ]
  then
    echo
    echo Starting instance s6c$mys6cn/$instidx
    init $instidx $args
    echo Instance s6c$mys6cn/$instidx pid $!
    # Sleep to let instance come up
    sleep 10
  else
    echo Instance $instidx not defined for host $hostname
  fi
done

# Zero out MISSEDPK counts
for instidx in ${instance_i[@]}
do
  echo Resetting MISSEDPK counts for s6c$mys6cn/$instidx
  hashpipe_check_status -I $instidx -k MISSEDPK -s 0
done

# Release NETHOLD
for instidx in ${instance_i[@]}
do
  echo Releasing NETHOLD for s6c$mys6cn/$instidx
  hashpipe_check_status -I $instidx -k NETHOLD -s 0
done
