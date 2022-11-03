import os
# import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from statistics import mean
# import glob
import numpy as np

std_state_delay_df = pd.DataFrame(columns=['End-to-End delay(s)', 'Network Solution', 'Network size'])
convergence_delay_df = pd.DataFrame(columns=['End-to-End delay(s)', 'Network Solution', 'Network size'])
convergence_df = pd.DataFrame(columns=['Convergence time (s)', 'Network Solution', 'Network size'])

app_int = 2
net_size_array = [3, 4, 5, 6, 7, 8]

sent_asn_start_sing = 's|as '
sent_asn_end_sign = ' as|ar'
recv_asn_start_sing = '|ar '
recv_asn_end_sing = ' ar|c'
seq_start_sing = 'd|s '
seq_end_sing = ' s|as'
addr_src_start_sing = '|sr '
addr_src_end_sing = ' sr|'
sent_port_start_sing = 'c|p '
sent_port_end_sing = ' p|'


msf_filename_key = 'msf-net-'


log_dir = "/home/fvg/contiki-ng/examples/6tisch/msf/statistic/log"
logfile_list = os.listdir(log_dir)
log_list = sorted(logfile_list)


def get_list_of_log_file(filename_key, list_to_search):
    net_log_list = []
    for i in list_to_search:
        if filename_key in i:
            net_log_list.append(i)
    # print(net_log_list)
    return net_log_list

# ########### delay analysis ###################
def e2e_delay(log_file_name, end_node_id):
    log_path = "/home/fvg/contiki-ng/examples/6tisch/msf/statistic/log/" + str(log_file_name)
    file = open(log_path, "r")
    input_lines = file.readlines()
    file.close()
    key_line = "11111111 "
    tot_rx_count1 = 0
    tot_rx_count2 = 0
    packet_num = 1000
    initial_drift = 300
    lat_critic_array1 = [None] * packet_num
    lat_critic_array2 = [None] * packet_num

    for line in input_lines:
        crtc_start_sing = line.find(addr_src_start_sing) + len(addr_src_start_sing)
        crtc_end_sing = line.find(addr_src_end_sing)
        if key_line in line and line[crtc_start_sing:crtc_end_sing] == ('0' + str(end_node_id)):
            sent_asn_s = line.find(sent_asn_start_sing) + len(sent_asn_start_sing)
            sent_asn_e = line.find(sent_asn_end_sign)
            rcv_asn_s = line.find(recv_asn_start_sing) + len(recv_asn_start_sing)
            rcv_asn_e = line.find(recv_asn_end_sing)
            seq_s = line.find(seq_start_sing) + len(seq_start_sing)
            seq_e = line.find(seq_end_sing)
            sent_port_s = line.find(sent_port_start_sing) + len(sent_port_start_sing)
            sent_port_e = line.find(sent_port_end_sing)
            if int(line[seq_s:seq_e]) < packet_num + 1 and line[sent_port_s:sent_port_e] == "1020":
                tot_rx_count1 += 1
                lat_critic_array1[int(line[seq_s:seq_e]) - 1] = (int(line[rcv_asn_s:rcv_asn_e], 16) - int(line[sent_asn_s:sent_asn_e], 16)) * 10

            if int(line[seq_s:seq_e]) < packet_num + 1 and line[sent_port_s:sent_port_e] == "1030":
                tot_rx_count2 += 1
                lat_critic_array2[int(line[seq_s:seq_e]) - 1] = (int(line[rcv_asn_s:rcv_asn_e], 16) - int(line[sent_asn_s:sent_asn_e], 16)) * 10
    return lat_critic_array1


def split_converge_time_steady_state(e2e_delay_list):
    print(e2e_delay_list)
    convergence_point = None
    std_point = 500
    w = e2e_delay_list[std_point:std_point + 100]
    if not all(v is None for v in w):
        std_median = mean(d for d in w if d is not None)
        print(std_median * 2)
        for item in range(std_point):
            # tmp_win = e2e_delay_list[std_point - item - w_len: std_point - item]
            if e2e_delay_list[std_point - item] > (std_median * 2):
                convergence_point = std_point - item
                # print(std_point - i + w_len)
                # print(e2e_delay_list[0:std_point - i])
                # print(e2e_delay_list[std_point - i:std_point])
                break
    return convergence_point


for i in net_size_array:
    net_log_list = get_list_of_log_file(msf_filename_key + str(i), log_list)
    for j in net_log_list:
        delay_array = e2e_delay(j, i)
        print(j)
        if not all(v is None for v in delay_array):
            conv_point = split_converge_time_steady_state(delay_array)
            print(conv_point)
            if conv_point is not None:
                conv_delay = None
                std_delay = None
                if not all(s is None for s in delay_array[0:conv_point]):
                    conv_delay = mean(d for d in delay_array[0:conv_point] if d is not None)

                if not all(s is None for s in delay_array[conv_point:len(delay_array)]):
                    std_delay = mean(d for d in delay_array[conv_point:len(delay_array)] if d is not None)

                temp_conv_point = [conv_point * app_int, 'MSF', i]
                df_cp_len = len(convergence_df)
                convergence_df.loc[df_cp_len] = temp_conv_point

                if not (conv_delay is None):
                    temp_conv_delay = [conv_delay/1000, 'MSF', i]
                    df_cd_len = len(convergence_delay_df)
                    convergence_delay_df.loc[df_cd_len] = temp_conv_delay

                if not (std_delay is None):
                    temp_std_delay = [std_delay/1000, 'MSF', i]
                    df_cp_len = len(std_state_delay_df)
                    std_state_delay_df.loc[df_cp_len] = temp_std_delay


sns.set(font_scale=3.2)
sns.set_style({'font.family': 'serif'})
ax_sdn_pdr = sns.catplot(kind='violin', data=convergence_df, x='Network size', y='Convergence time (s)',
                         hue='Network Solution', palette="tab10", cut=0, scale='width',
                         saturation=1, legend=False, height=10, aspect=1.8)
#ax_sdn_pdr.set(ylim=(0, 102))
# plt.subplots_adjust(top=.8)
plt.legend(loc='upper center', ncol=3, fontsize='24', bbox_to_anchor=[0.5, 1.26])
# plt.yticks(range(0,110,20))
# ax_sdn_pdr.fig.set_figwidth(20)
# ax_sdn_pdr.fig.set_figheight(10)
plt.savefig('Convergence_time.pdf', bbox_inches='tight')


sns.set(font_scale=3.2)
sns.set_style({'font.family': 'serif'})
ax_sdn_pdr = sns.catplot(kind='violin', data=convergence_delay_df, x='Network size', y='End-to-End delay(s)',
                         hue='Network Solution', palette="tab10", cut=0, scale='width',
                         saturation=1, legend=False, height=10, aspect=1.8)
#ax_sdn_pdr.set(ylim=(0, 102))
# plt.subplots_adjust(top=.8)
plt.legend(loc='upper center', ncol=3, fontsize='24', bbox_to_anchor=[0.5, 1.26])
# plt.yticks(range(0,110,20))
# ax_sdn_pdr.fig.set_figwidth(20)
# ax_sdn_pdr.fig.set_figheight(10)
plt.savefig('delay_convergence.pdf', bbox_inches='tight')


sns.set(font_scale=3.2)
sns.set_style({'font.family': 'serif'})
ax_sdn_pdr = sns.catplot(kind='violin', data=std_state_delay_df, x='Network size', y='End-to-End delay(s)',
                         hue='Network Solution', palette="tab10", cut=0, scale='width',
                         saturation=1, legend=False, height=10, aspect=1.8)
#ax_sdn_pdr.set(ylim=(0, 102))
# plt.subplots_adjust(top=.8)
plt.legend(loc='upper center', ncol=3, fontsize='24', bbox_to_anchor=[0.5, 1.26])
# plt.yticks(range(0,110,20))
# ax_sdn_pdr.fig.set_figwidth(20)
# ax_sdn_pdr.fig.set_figheight(10)
plt.savefig('delay_std_state.pdf', bbox_inches='tight')

e2e_lat_array = []
for net in range(1):
    e2e_lat_array.append(e2e_delay(log_list[net], 4))
    plt.plot(e2e_lat_array[net], label='Net'+str(net+3))


plt.xlabel("Packet seq-num")
plt.ylabel("End-to-end delay (ms)")
plt.legend()
plt.savefig('msf_pdr11.png', bbox_inches='tight')


# ################# 2APP code ###############
#
# initial_drift = 300
# packet_num = 1000
#
# def e2e_delay(log_file_name, end_node_id):
#     log_path = "/home/fvg/contiki-ng/examples/6tisch/msf/statistic/log/" + str(log_file_name)
#     file = open(log_path, "r")
#     input_lines = file.readlines()
#     file.close()
#     key_line = "11111111 "
#     tot_rx_count1 = 0
#     tot_rx_count2 = 0
#     lat_critic_array1 = [None] * packet_num
#     lat_critic_array2 = [None] * packet_num
#
#     for line in input_lines:
#         crtc_start_sing = line.find(addr_src_start_sing) + len(addr_src_start_sing)
#         crtc_end_sing = line.find(addr_src_end_sing)
#         if key_line in line and line[crtc_start_sing:crtc_end_sing] == ('0' + str(end_node_id)):
#             sent_asn_s = line.find(sent_asn_start_sing) + len(sent_asn_start_sing)
#             sent_asn_e = line.find(sent_asn_end_sign)
#             rcv_asn_s = line.find(recv_asn_start_sing) + len(recv_asn_start_sing)
#             rcv_asn_e = line.find(recv_asn_end_sing)
#             seq_s = line.find(seq_start_sing) + len(seq_start_sing)
#             seq_e = line.find(seq_end_sing)
#             sent_port_s = line.find(sent_port_start_sing) + len(sent_port_start_sing)
#             sent_port_e = line.find(sent_port_end_sing)
#             if int(line[seq_s:seq_e]) < packet_num + 1 and line[sent_port_s:sent_port_e] == "1020":
#                 tot_rx_count1 += 1
#                 lat_critic_array1[int(line[seq_s:seq_e]) - 1] = ((int(line[rcv_asn_s:rcv_asn_e], 16) - int(line[sent_asn_s:sent_asn_e], 16)) * 10) / 1000
#
#             if int(line[seq_s:seq_e]) < packet_num + 1 and line[sent_port_s:sent_port_e] == "1030":
#                 tot_rx_count2 += 1
#                 lat_critic_array2[int(line[seq_s:seq_e]) - 1] = ((int(line[rcv_asn_s:rcv_asn_e], 16) - int(line[sent_asn_s:sent_asn_e], 16)) * 10) / 1000
#     return lat_critic_array1, lat_critic_array2
#
#
# e2e_lat_array = []
# app1, app2 = e2e_delay(log_list[0], 3)
# e2e_lat_array.append(app1)
# plt.plot(e2e_lat_array[0], label='APP 1 (2 s)')
# e2e_lat_array.append(app2)
# plt.plot(e2e_lat_array[1], label='APP 2 (1 s)')
#
# print(len([x for x in app1 if x is not None]) / packet_num)
# print((len([x for x in app2 if x is not None]) - 1) / (packet_num - initial_drift))
#
# plt.xlabel("Packet seq-num")
# plt.ylabel("End-to-end delay (s)")
# plt.legend()
# plt.savefig('end2end-delay-2app.png', bbox_inches='tight')
#

# print(split_converge_time_steady_state(e2e_lat_array[5]))
