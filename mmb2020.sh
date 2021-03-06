#!/bin/bash

## Run simulations

# Note:
# Independent replications are not really necessary (we could also generate 5*10^5 websites in a single run) but simplify running multiple simulations in parallel.
#
# Simulations with 500*1000 websites takes quite some time and pdf files generated by R become very large. Use ns-3 optimized built. Default value set to 5*1000 websites for now. 
INDEP_REPLICATIONS=5

for idxLink in  "--rDsl=1Mbps --dDsl=15ms"  "--rSat=20Mbps --dSat=300ms"  "--rDsl=1Mbps --dDsl=15ms --rSat=20Mbps --dSat=300ms"  "--rDsl=20Mbps --dDsl=15ms"; do
  for idxMode in s p h; do
    for idxRun in `seq 1 $INDEP_REPLICATIONS`; do
      echo "Running simulation ${idxLink}, mode ${idxMode}, runNumber ${idxRun}"
      ./waf --run "TMCv4_mmb2020 --runNumber=${idxRun} ${idxLink} --tranGiaMode=${idxMode} --ns3::TcpSocket::SegmentSize=1448 --ns3::TcpSocket::SndBufSize=8000000 --ns3::TcpSocket::RcvBufSize=8000000"
    done
  done
done


echo -e "\n\n"


# Merge files:
# TranGiaClient for page load times
# TmcPepRight for DSL/Sat statistics
echo "link,mode,startTime,nrMainObjects,mainObjectSizes,mainObjectsTotalSize,nrEmbObjects,embObjectSizes,embObjectsTotalSize,finishTime,readingTime" > mmb2020_linkAll_modeAll_tranGiaClient.csv
echo "link,mode,connId,srcIp,dstIp,srcPort,dstPort,sizeTer,sizeSat,sizeTotal,sizePending" > mmb2020_linkAll_modeAll_tmcPepRight.csv

for idxLink in  dsl1Mbps15ms_sat  dsl_sat20Mbps300ms  dsl1Mbps15ms_sat20Mbps300ms  dsl20Mbps15ms_sat; do
  for idxMode in s p h; do
    for idxRun in `seq 1 $INDEP_REPLICATIONS`; do
      echo "Wrting csv file for ${idxLink}, mode ${idxMode}, runNumber ${idxRun}"

      # TranGiaClient: remove header and add link/mode prefix
      tail -n+2 mmb2020_link_${idxLink}_mode${idxMode}_tranGiaClient_run${idxRun}.csv |sed -e "s/^/${idxLink},${idxMode},/" >> mmb2020_linkAll_modeAll_tranGiaClient.csv

      # TmcPepRight: remove header and add link/mode prefix
      tail -n+2 mmb2020_link_${idxLink}_mode${idxMode}_tmcPepRight_run${idxRun}.csv |sed -e "s/^/${idxLink},${idxMode},/" >> mmb2020_linkAll_modeAll_tmcPepRight.csv
    done
  done
done




