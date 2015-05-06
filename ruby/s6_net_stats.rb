#!/usr/bin/env ruby

# Monitore SERENDIP6 network statistics

require 'hashpipe'

STDOUT.sync = true

# Get instance_id
instance_id = Integer(ARGV[0]) rescue 0
iters = Integer(ARGV[1]) rescue 120

# Connect to Hashpipe status buffer
begin
    stat = Hashpipe::Status.new(instance_id, false)
rescue
    puts "Error connecting to status buffer for instance #{instance_id}"
    exit 1 
end

FIELDS = %w[
  NETBKOUT MISSEDPK NETDROPS
  NETWATNS NETRECNS NETPRCNS
  NETWATMX NETRECMX NETPRCMX
  NETGBPS
]

i = 1
prev_netbkout = nil
while iters != 0
  values = FIELDS.map {|k| stat.hgets(k)}
  if values[0] != prev_netbkout
    printf "%3d %s %s %s %.3f %.3f %.3f %6d %5d %6d %.1f\n", i, *values
    prev_netbkout = values[0]
    i += 1
    iters -= 1
  end

  sleep 0.5
end
