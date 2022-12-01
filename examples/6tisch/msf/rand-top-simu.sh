#!/bin/bash

#########################################################################
# clean log directory
if [ -d "statistic/log" ] 
then
    rm -r statistic/log 
fi

if [ ! -d "statistic/log" ] 
then
    mkdir statistic/log 
    echo "create log directory"
fi


# remove COOJA.testlog file
if [ -f "COOJA.testlog" ] 
then
    rm COOJA.testlog
fi

######################################################################### 
NODE_NUM_LIST=(10 20 30 40 50)                        # network size included Sink

SRVR_NUM=1
ITER_PER_CONF=1                           # number of iteration for each config.csc (max = 10)
LQR_LIST=(0.0)                            # link quality metric of cooja: means at the max tx-range link quality is 0 (decresing by distance)

LIST_SIZE=${#NODE_NUM_LIST[*]}            # traffic array len
LQR_LIST_SIZE=${#LQR_LIST[*]}


k=0
while [[ $k -le $(($LQR_LIST_SIZE-1)) ]]
do
    j=0
    while [[ $j -le $(($LIST_SIZE-1)) ]]
    do 
        i=1
        while [[ $i -le $ITER_PER_CONF ]]
        do
            python3 ~/contiki-ng/examples/6tisch/msf/topo-rand-config/main.py \
            node_num=[${NODE_NUM_LIST[j]}] \
            server_num=[$SRVR_NUM] \
            relay_node_num=[0] \
            end_node_num=[$((${NODE_NUM_LIST[j]}-SRVR_NUM))] \
            tx_range=[100.0] \
            intf_range=[150.0] \
	    tx_success=[1.0] \
	    rx_success=[${LQR_LIST[k]}] \
	    itr=[$i] \
            sim_time_sdn=[6000000]

            java -Xshare:on -jar ../../../tools/cooja/dist/cooja.jar -nogui=config.csc -contiki=../../../
        
        
            if [ -f "COOJA.testlog" ] 
            then
                mv ~/contiki-ng/examples/6tisch/msf/COOJA.testlog \
	        ~/contiki-ng/examples/6tisch/msf/statistic/log/msf-net-${NODE_NUM_LIST[j]}-itr-$i-lqr${LQR_LIST[k]}.testlog
            fi
        
            i=$(($i+1))
        done
    
        j=$(($j+1))
    done
    
    k=$(($k+1))
done




