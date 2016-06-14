#! /bin/tcsh

date

#foreach i (s6c0 s6c1 s6c2 s6c3) # AO
foreach i (s6c1 s6c2 s6c3)       # GBT
    echo $i
    echo "    killing old s6 (if any)..."
    ssh $i /usr/local/bin/s6_stop.sh 
    #echo "    starting redis gateway..."
    #ssh $i  hashpipe_redis_gateway.rb
    echo "    starting s6..."
    ssh $i /usr/local/bin/s6_restart.sh
    echo " "
end
