import random
import math


#################################################################

def random_gen(minimum, maximum):
    return minimum + (maximum - minimum) * random.random()


def set_nodes_position(nodes_num, x_radius, y_radius, tx_range):
    find_loc = 1
    pos_array = nodes_num * [2*[0]]
    while find_loc < nodes_num:
        p1 = [random_gen(-x_radius, x_radius), random_gen(-y_radius, y_radius)]
        for i in range(nodes_num):
            if i < find_loc and math.sqrt(((p1[0] - pos_array[i][0]) ** 2) + ((p1[1] - pos_array[i][1]) ** 2)) < (tx_range - 3):
                if pos_array[0][0] == 0 and pos_array[0][1] == 0:
                    pos_array[find_loc-1] = p1
                    break
                else:
                    pos_array[find_loc] = p1
                    find_loc += 1
                    break

    return pos_array


def get_val_from_input_array(input_array, param):
    for i in input_array:
        if param in i:
            start = i.find("[") + len("[")
            end = i.find("]")
            s = i[start:end]
            dot = s.find(".")
            if dot > 0:
                return float(s)
            else:
                return int(s)


# def create_network_graph(nodes_num, x_radius, y_radius, tx_range):
#     max_nbr = 8
#     max_hop = 6
#     max_iter = 30000
#     found_node = 0
#     pos_array = []
#     hop_index_array = []
#     p1 = [0, 0]
#     pos_array.append(p1)
#     hop_index_array.append(0)
#     found_node += 1
#     iter = 0
#     while found_node < nodes_num and iter < max_iter:
#         iter += 1
#         p1 = [random_gen(-x_radius, x_radius), random_gen(-y_radius, y_radius)]
#         for i in range(len(pos_array)):
#             if math.sqrt(((p1[0] - pos_array[i][0]) ** 2) + ((p1[1] - pos_array[i][1]) ** 2)) < (tx_range - 2):
#                 pos_array.append(p1)
#                 exceed_max_nbr = 0
#                 for j in range(len(pos_array) - 1):
#                     nbr_num = 0
#                     for k in range(len(pos_array)):
#                         if j != k and math.sqrt(((pos_array[j][0] - pos_array[k][0]) ** 2) + ((pos_array[j][1] - pos_array[k][1]) ** 2)) < (tx_range - 2):
#                             nbr_num += 1
#                     if nbr_num > max_nbr:
#                         exceed_max_nbr = 1
#                         pos_array.pop()
#                         break
#
#
#                 if exceed_max_nbr == 0:
#                     max_nbr_hop = 0
#                     for n in range(len(pos_array) - 1):
#                         if math.sqrt(((p1[0] - pos_array[n][0]) ** 2) + ((p1[1] - pos_array[n][1]) ** 2)) < (tx_range - 2) and hop_index_array[n] > max_nbr_hop:
#                             max_nbr_hop = hop_index_array[n]
#
#                     if max_nbr_hop < max_hop:
#                         hop_index_array.append(max_nbr_hop + 1)
#                     else:
#                         exceed_max_nbr = 1
#                         pos_array.pop()
#
#
#
#                 if exceed_max_nbr == 0:
#                     found_node += 1
#                 break
#     num_max_hop_in_array = 0
#     for s in range(len(hop_index_array)):
#         if hop_index_array[s] == max_hop:
#             num_max_hop_in_array += 1

def create_network_graph(nodes_num, tx_range):
    pos_array = [[0] * 2 for _ in range(nodes_num)]
    last_y = 0
    y_dist = 40
    for i in range(nodes_num):
        pos_array[i][1] = last_y
        last_y = last_y + y_dist
    return pos_array
