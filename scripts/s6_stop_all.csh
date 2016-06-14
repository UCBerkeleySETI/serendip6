#! /bin/tcsh

date

#foreach i (s6c0 s6c1 s6c2 s6c3)    # AO
foreach i (s6c1 s6c2 s6c3)          # GBT
    echo $i"..."
    ssh $i /usr/local/bin/s6_stop.sh
end
