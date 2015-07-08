#! /bin/tcsh

date

#foreach i (s6c0 s6c1 s6c2 s6c3)
foreach i (s6c1 s6c2 s6c3)
    echo ""
    ssh $i /usr/local/bin/s6_stop.sh
end
