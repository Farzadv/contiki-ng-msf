#!/bin/bash


#########################################################################
# clean log directory
if [ -d "statistic/log-density" ] 
then
    rm -r statistic/log-density 
fi

if [ ! -d "statistic/log-density" ] 
then
    mkdir statistic/log-density 
    echo "create log density"
fi


# remove COOJA.testlog file
if [ -f "COOJA.testlog" ] 
then
    rm COOJA.testlog
fi

######################################################################### 
NODE_NUM_LIST=(25)                        # network size included Sink
NODE_END_APP=(24)                       # number of critic nodes
SRVR_NUM=1
ITER_PER_CONF=1                           # number of iteration for each config.csc
LQR_LIST=(0.84)
RADIUS_LIST=(20.0)                 # given network size=25 & max_hop=4 and max_nbr=10 and tx_range=50 --> max coef_of_radius=8 => radius=200

LIST_SIZE=${#NODE_NUM_LIST[*]}            # traffic array len
LQR_LIST_SIZE=${#LQR_LIST[*]}
RADIUS_LIST_SIZE=${#RADIUS_LIST[*]}       


k=0
while [[ $k -le $(($LQR_LIST_SIZE-1)) ]]
do
    j=0
    while [[ $j -le $(($LIST_SIZE-1)) ]]
    do 
        m=0
	while [[ $m -le $(($RADIUS_LIST_SIZE-1)) ]]
	do
            i=1
            while [[ $i -le $ITER_PER_CONF ]]
            do
                python3 /home/fvg/contiki-ng/examples/6tisch/msf/topo-density-config/main.py \
                node_num=[${NODE_NUM_LIST[j]}] \
                server_num=[$SRVR_NUM] \
                relay_node_num=[$((${NODE_NUM_LIST[j]}-${NODE_END_APP[j]}-SRVR_NUM))] \
                end_node_num=[${NODE_END_APP[j]}] \
                tx_range=[50] \
	        tx_success=[1.0] \
	        rx_success=[${LQR_LIST[k]}] \
	        itr=[$i] \
	        x_radius=[${RADIUS_LIST[m]}] \
		y_radius=[${RADIUS_LIST[m]}] \
                sim_time_sdn=[6000000]

                java -Xshare:on -jar ../../../tools/cooja/dist/cooja.jar -nogui=config.csc -contiki=../../../
        
        
                if [ -f "COOJA.testlog" ] 
                then
                    mv /home/fvg/contiki-ng/examples/6tisch/msf/COOJA.testlog \
	            /home/fvg/contiki-ng/examples/6tisch/msf/statistic/log-density/msf-net-${NODE_NUM_LIST[j]}-itr-$i-lqr${LQR_LIST[k]}-radius-${RADIUS_LIST[m]}.testlog
                fi
        
                i=$(($i+1))
            done
            
            m=$(($m+1))
						
	done    
    
        j=$(($j+1))
    done
    
    k=$(($k+1))
done




