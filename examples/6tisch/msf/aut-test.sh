#!/bin/bash


#########################################################################
# check if simulation directory exists? if so, remove it
if [ -d "simulation-log" ] 
then
    echo "Directory simulation-log exists."
    rm -r simulation-log
    echo "current simulation-log directory is removed!"
else
    echo "simulation-log directory does not exists."
fi


# create simulation directory
if [ ! -d "simulation-log" ] 
then
    mkdir simulation-log
    echo "create a new fresh simulation-log directory" 

fi

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
NODE_NUM_LIST=(2 3 4)                        # network size included Sink
NODE_END_APP=(1 2 3)                       # number of critic nodes
SRVR_NUM=1
ITER_PER_CONF=10                           # number of iteration for each config.csc
LQR_LIST=(0.95 0.84 0.71)

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
            python3 /home/fvg/contiki-ng/examples/6tisch/msf/topo-chain-config/main.py \
            node_num=[${NODE_NUM_LIST[j]}] \
            server_num=[$SRVR_NUM] \
            relay_node_num=[$((${NODE_NUM_LIST[j]}-${NODE_END_APP[j]}-SRVR_NUM))] \
            end_node_num=[${NODE_END_APP[j]}] \
            tx_range=[50] \
	    tx_success=[1.0] \
	    rx_success=[${LQR_LIST[k]}] \
            sim_time_sdn=[6000000]

            java -Xshare:on -jar ../../../tools/cooja/dist/cooja.jar -nogui=config.csc -contiki=../../../
        
        
            if [ -f "COOJA.testlog" ] 
            then
                mv /home/fvg/contiki-ng/examples/6tisch/msf/COOJA.testlog \
	        /home/fvg/contiki-ng/examples/6tisch/msf/statistic/log/msf-net-${NODE_NUM_LIST[j]}-itr-$i-lqr${LQR_LIST[k]}.testlog
            fi
        
            i=$(($i+1))
        done
    
        j=$(($j+1))
    done
    
    k=$(($k+1))
done




