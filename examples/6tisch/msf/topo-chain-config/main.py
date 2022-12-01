import sys
import shutil
import fileinput
import os
from func_lib import *
from conf_tmp import *
import random

print('====> INPUT PARAMITER    =   ', str(sys.argv))
param_input = sys.argv

user_home_path = os.path.expanduser('~')

node_num = get_val_from_input_array(param_input, "node_num")
server_num = get_val_from_input_array(param_input, "server_num")
relay_node_num = get_val_from_input_array(param_input, "relay_node_num")
end_node_num = get_val_from_input_array(param_input, "end_node_num")
tx_range = get_val_from_input_array(param_input, "tx_range")  # meter
tx_success = get_val_from_input_array(param_input, "tx_success")
rx_success = get_val_from_input_array(param_input, "rx_success")
sim_time_sdn = get_val_from_input_array(param_input, "sim_time_sdn")

# node_num = 7
# server_num = 1
# relay_node_num = 5
# end_node_num = 1
# tx_range = 50  # meter
# sim_time_sdn = 1800000

mtype_num = []

if server_num != 0:
    mtype_num.append("mtype" + str(random.randint(100, 1000)))
else:
    mtype_num.append("zero_val")

if relay_node_num != 0:
    mtype_num.append("mtype" + str(random.randint(100, 1000)))
else:
    mtype_num.append("zero_val")

if end_node_num != 0:
    mtype_num.append("mtype" + str(random.randint(100, 1000)))
else:
    mtype_num.append("zero_val")
print("====> MTYPE VALUES    =    ", mtype_num)


position_array = []
position_array = create_network_graph(node_num, tx_range)
print("====> NODE's LOCATION ARRAY    =    ", position_array)

sdn_template = base_template


def set_mote_type(conf_key, replace_conf, srv_exis, relay_node_exis, end_node_exis):
    mote_type = ''
    cooja_mtype = 0
    if srv_exis > 0:
        mote_type += replace_conf
        cooja_mtype += 1
        mote_type = mote_type.replace("MTYPE_NUM", mtype_num[0])
        mote_type = mote_type.replace("COOJA_MOTE_TYPE", "Cooja Mote Type #" + str(cooja_mtype))
        mote_type = mote_type.replace("FIRMWAR_DIR", "[CONTIKI_DIR]/examples/6tisch/msf/msf-root.c")
        mote_type = mote_type.replace("MAKE_COMMAND", "make msf-root.cooja TARGET=cooja")

    if relay_node_exis > 0:
        mote_type += replace_conf
        cooja_mtype += 1
        mote_type = mote_type.replace("MTYPE_NUM", mtype_num[1])
        mote_type = mote_type.replace("COOJA_MOTE_TYPE", "Cooja Mote Type #" + str(cooja_mtype))
        mote_type = mote_type.replace("FIRMWAR_DIR", "[CONTIKI_DIR]/examples/6tisch/msf/msf-node-relay.c")
        mote_type = mote_type.replace("MAKE_COMMAND", "make msf-node-relay.cooja TARGET=cooja")

    if end_node_exis > 0:
        mote_type += replace_conf
        cooja_mtype += 1
        mote_type = mote_type.replace("MTYPE_NUM", mtype_num[2])
        mote_type = mote_type.replace("COOJA_MOTE_TYPE", "Cooja Mote Type #" + str(cooja_mtype))
        mote_type = mote_type.replace("FIRMWAR_DIR", "[CONTIKI_DIR]/examples/6tisch/msf/msf-node.c")
        mote_type = mote_type.replace("MAKE_COMMAND", "make msf-node.cooja TARGET=cooja")

    return sdn_template.replace(conf_key, mote_type)


def add_motes(conf_key, replace_conf):
    mote_type = ''
    node_id = 0
    if conf_key in sdn_template:
        p = -1
        for i in range(server_num):
            mote_type += replace_conf
            node_id += 1
            p += 1
            mote_type = mote_type.replace("X_POSITION", str(position_array[p][0]))
            mote_type = mote_type.replace("Y_POSITION", str(position_array[p][1]))
            mote_type = mote_type.replace("Z_POSITION", str(0.0))
            mote_type = mote_type.replace("MOTE_ID", str(node_id))
            mote_type = mote_type.replace("MOT_TYPE", mtype_num[0])

        for i in range(relay_node_num):
            mote_type += replace_conf
            node_id += 1
            p += 1
            mote_type = mote_type.replace("X_POSITION", str(position_array[p][0]))
            mote_type = mote_type.replace("Y_POSITION", str(position_array[p][1]))
            mote_type = mote_type.replace("Z_POSITION", str(0.0))
            mote_type = mote_type.replace("MOTE_ID", str(node_id))
            mote_type = mote_type.replace("MOT_TYPE", mtype_num[1])

        for i in range(end_node_num):
            mote_type += replace_conf
            node_id += 1
            p += 1
            mote_type = mote_type.replace("X_POSITION", str(position_array[p][0]))
            mote_type = mote_type.replace("Y_POSITION", str(position_array[p][1]))
            mote_type = mote_type.replace("Z_POSITION", str(0.0))
            mote_type = mote_type.replace("MOTE_ID", str(node_id))
            mote_type = mote_type.replace("MOT_TYPE", mtype_num[2])

        return sdn_template.replace(conf_key, mote_type)


def set_radio_medium(conf_key, replace_conf):
    mote_type = ''
    if conf_key in sdn_template:
        mote_type += replace_conf
        mote_type = mote_type.replace("TX_RANGE", str(50.0))
        mote_type = mote_type.replace("INT_RANGE", str(100.0))
        mote_type = mote_type.replace("TX_SUCCESS", str(tx_success))
        mote_type = mote_type.replace("RX_SUCCESS", str(rx_success))

        return sdn_template.replace(conf_key, mote_type)


def add_mote_id(conf_key, replace_conf):
    mote_type = ''
    node_id = -1
    if conf_key in sdn_template:
        for i in range(node_num):
            mote_type += replace_conf
            node_id += 1
            mote_type = mote_type.replace("MOTE_ID2", str(node_id))

        return sdn_template.replace(conf_key, mote_type)


def set_simulation_time(conf_key):
    if conf_key in sdn_template:
        return sdn_template.replace(conf_key, str(sim_time_sdn))


def set_random_seed(seed_key):
    if seed_key in sdn_template:
        rand_seed = random.randint(1, 9)
        for i in range(5):
            rand_seed = (rand_seed * 10) + random.randint(1, 9)
        replace_word = "    <randomseed>" + str(rand_seed) + "</randomseed>\n"
        return sdn_template.replace(seed_key, replace_word)


sdn_template = set_random_seed("OKMNJI")
sdn_template = set_radio_medium("QAZWSX", radio_medium)
sdn_template = set_mote_type("QWERTY", mote_type_id, server_num, relay_node_num, end_node_num)
sdn_template = add_motes("ASDFGH", mote_spec)
sdn_template = add_mote_id("ZXCVBN", mote_id)
sdn_template = set_simulation_time("EDCRFV")


file_w = open(user_home_path + "/contiki-ng/examples/6tisch/msf/topo-chain-config/config.csc", "w+")
file_w.write(sdn_template)
file_w.close()

if os.path.exists(user_home_path + "/contiki-ng/examples/6tisch/msf/config.csc"):
    os.remove(user_home_path + "/contiki-ng/examples/6tisch/msf/config.csc")

newPath = shutil.copy(user_home_path + "/contiki-ng/examples/6tisch/msf/topo-chain-config/config.csc", user_home_path + "/contiki-ng/examples/6tisch/msf")
