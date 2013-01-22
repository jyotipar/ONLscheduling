#
#  Copyright (c) 2012,2013 Jyoti Parwatikar and Washington University in St. Louis.
#  All rights reserved
#
#  Distributed under the terms of the GNU General Public License v3
#

#! /usr/bin/python


import networkx
import argparse
#import pygraphviz as pgv
import matplotlib.pyplot as mplot
import math


#read in command line arguments to get testbed graph file
aparser = argparse.ArgumentParser(description="read in testbed graph file, process reservations and display testbed with allocations")
aparser.add_argument("-f", help="input user graph files in GraphML", nargs='+')
aparser.add_argument("-ugraphs", help="show user mini graphs", action="store_true")
aparser.add_argument("-verbose", help="turn debugging messages on integer greater than 0")
aparser.add_argument("-show_failure", help="show partial mapping if reservation fails", action="store_true")
aparser.add_argument("-no_split", help="don't split vswitches", action="store_true")
aparser.add_argument("-pmap", help="print the final mapping", action="store_true")
args = aparser.parse_args()

debug_level = 0
split_vswitches = True
show_partial_mapping = False
print_final_mapping = False
if args.pmap:
    print_final_mapping = True
if args.show_failure:
    show_partial_mapping = True
if args.verbose:
    debug_level = int(args.verbose)
if args.no_split:
    split_vswitches = False

#create testbed graph from GraphML file

#g = networkx.read_graphml(args.f)
testbed = networkx.MultiGraph()
testbed_viz = networkx.MultiGraph() #visualization of testbed without ports

class Node:
    def __init__(self, nid, hwtype):
        self.label = nid
        self.hwtype = hwtype
        self.res_id = 'free'
        self.index = 0
        self.parent = None
        self.ports = []
    def is_leaf(self):
        if self.hwtype == 'vswitch' or self.hwtype.startswith('sw'):
            return False
        else:
            return True

class Edge:
    def __init__(self, n1, n2, cap):
        self.capacity = cap
        self.node1 = n1
        self.node2 = n2
        self.rload = 0
        self.lload = 0
        self.id = 0
        self.inter_cluster = False
    def unallocated_right_bw(self):
        return (self.capacity - self.rload)
    def unallocated_left_bw(self):
        return (self.capacity - self.lload)

class InterClusterEdge(Edge):
    def __init__(self, n1, n2, cap):
        Edge.__init__(self, n1, n2, cap)
        self.inter_cluster = True

class ReservationNode(Node):
    def __init__(self, nid, hwtype):
        Node.__init__(self, nid, hwtype)
        self.mapped_to = None
        self.cost = 0
        #self.out_cost = 0
        #self.in_cost = 0
    def get_cost(self):
        return self.cost
        #if self.out_cost > self.in_cost:
         #   return self.out_cost
        #else:
         #   return self.in_cost
    def unmap(self):
        rnode = self.mapped_to
        if self.mapped_to:
            self.mapped_to = None
            rnode.res_id = 'free'
        for p in self.ports:
            p.unmap()
    def map_to(self, rnode):
        if self.mapped_to:
            if self.mapped_to == rnode:
                return
            self.mapped_to.res_id = 'free'
        self.mapped_to = rnode
        if debug_level > 1:
            print 'map ' + self.label + ' to ' + rnode.label
        rnode.res_id = self.res_id
        if self.parent and self.parent.mapped_to == None:
            self.parent.map_to(rnode.parent)
        if not (self.parent or rnode.parent or (self.hwtype == 'vswitch')):
            for p in self.ports:
                p.map_to(rnode.ports[p.index])      

class Path:
    def __init__(self):
        self.edges = []
        self.load = 0
    def add_edge(self, edge):
        self.edges.append(edge)
    def add_load(self, l):
        self.load += l
    def clear(self):
        self.edges = []
        self.load = 0
    def length(self):
        return len(self.edges)
   

class ReservationEdge(Edge):
    def __init__(self, n1, n2, cap):
        Edge.__init__(self, n1, n2, cap)
        self.path = Path()
        self.cost = 0
        self.intercluster_cost = 0
    def calculate_costs(self):
        rtraffic = self.rload
        if rtraffic > 10:
            rtraffic = 10
        ltraffic = self.lload
        if ltraffic > 10:
            ltraffic = 10
        self.cost = rtraffic + ltraffic + (math.fabs(rtraffic - ltraffic)/2)
    def calculate_intercluster_cost(self):
        self.intercluster_cost = 0
        for e in self.path.edges:
            if e.inter_cluster:
                self.intercluster_cost += self.cost
    def is_mapped(self):
        if len(self.path.edges) > 0:
            return True
        return False
    def print_mapping(self):
        path_str = ''
        first = True
        for e in self.path.edges:
            if first:
                first = False
            else:
                path_str = path_str + ', '
            path_str = path_str + '(' + e.node1.label + ',' + e.node2.label + ')'
        print '  user edge:(' + self.node1.label + ',' + self.node2.label + ') mapped to:(' + path_str + ')'

class ReservationSubnet:
    def __init__(self, nodes, graph):
        self.nodes = nodes
        self.edges = []
        for n in nodes:
            sn_edges = graph.edges(n)
            for e in sn_edges:
                if e in graph.edges():
                    self.edges.append(e)    
            
class Reservation:
    def __init__(self, rid, rg_file, color):
        self.mapping_failed = False
        self.label = rid
        self.file = rg_file
        self.ice_cost = 0
        #read reservation from file 
        g = networkx.read_graphml(rg_file)
        self.type_map = {}
        self.graph = networkx.MultiGraph()
        #build reservation topology from read in graph
        self.host_cost = 0
        for n in g.nodes_iter():
            tmp = n.split('.')
            nlabel = tmp[0] + '.' + tmp[1]
            new_node = self.get_node(nlabel, tmp[0])
            new_node.index = int(tmp[1])
            if new_node.hwtype.startswith('pc'):
                if new_node.hwtype == 'pc1':
                    self.host_cost += 1
                else:
                    self.host_cost += 10
            if len(tmp) > 2:
                port = ReservationNode(n, tmp[0])
                port.index = int(tmp[2])
                port.parent = new_node
                new_node.ports.append(port)
                port.res_id = self.label
                self.graph.add_node(port)
            if tmp[0] == 'vswitch':
                self.graph.add_node(new_node)
        for e in g.edges_iter():
            n1 = self.get_edge_node(e[0])
            n2 = self.get_edge_node(e[1])
            re = ReservationEdge(n1, n2, 0)
            self.graph.add_edge(re.node1, re.node2, info=re)
            #self.graph[n1][n2]['info'] = re
            #re = ReservationEdge(n2, n1, 0)
            #self.graph.add_edge(re.node1, re.node2, info=re)
        self.color = color
        self.subnets = []
        subnets = networkx.connected_components(self.graph)
        for sn in subnets:
            elem = ReservationSubnet(sn, self.graph)
            self.subnets.append(elem)

    def unmap(self):
        for type_elem in self.type_map.iteritems():
            for n in type_elem[1]:
                n.unmap()

    def recompute_subnets(self):
        self.subnets = []
        subnets = networkx.connected_components(self.graph)
        for sn in subnets:
            elem = ReservationSubnet(sn, self.graph)
            self.subnets.append(elem)

    def visualize_mapping(self):
        viz_graph = networkx.MultiGraph()
        for type_elem in self.type_map.iteritems():
            for n in type_elem[1]:
                if n.mapped_to:
                    viz_graph.add_node(n.mapped_to)
                else:
                    print 'error: visualize_mapping node (' + n.label + ') is not mapped'
        edge_info_att = networkx.get_edge_attributes(self.graph, 'info')
        for ue in edge_info_att.iteritems():
            for e in ue[1].path.edges:
                n1 = e.node1
                if n1.parent:
                    n1 = n1.parent    
                n2 = e.node2
                if n2.parent:
                    n2 = n2.parent  
                viz_graph.add_edge(n1, n2)  
        #node_shapes = []
        #node_sizes = []
        #node_list = []
        #for n in viz_graph.nodes():
            #node_list.append(n)
            #if n.hwtype.startswith('sw'):
                #node_shapes.append('s')
                #node_size.append(1000)
            #elif n.hwtype == 'pc1':
                #node_shapes.append('o')
                #node_size.append(20)
            #elif n.hwtype == 'pc8':
                #node_shapes.append('o')
                #node_size.append(100)
            #elif n.hwtype == 'pc48':
                #node_shapes.append('8')
                #node_size.append(100)
            #elif n.hwtype == 'ixp':
                #node_shapes.append('^')
                #node_size.append(300)
            #else:
                #node_shapes.append('v')
                #node_size.append(300)
        pos = networkx.graphviz_layout(viz_graph)
        networkx.draw_networkx_nodes(viz_graph, pos, node_color=self.color, node_size=50)
        labels = {}
        for n in viz_graph.nodes():
            labels[n] = n.label
        networkx.draw_networkx_labels(viz_graph, pos, labels) 
        networkx.draw_networkx_edges(viz_graph, pos)
 
    def get_subnet(self, node):
        for sn in self.subnets:
            if node in sn.nodes:
                return sn
        return None
   
    def get_edge_node(self, nlabel):
        for n in self.graph.nodes():
            if n.label == nlabel:
                return n

    def get_node(self, nlabel, hwtype):   
        if not (hwtype in self.type_map):
            self.type_map[hwtype] = []
        for n in self.type_map[hwtype]:
            if n.label == nlabel:
                return n
        node = ReservationNode(nlabel, hwtype)
        node.res_id = self.label
        self.type_map[hwtype].append(node)
        return node

    def sum_edge_links(self, subnet, node, nodes_seen):
        rtn = 0
        ntp = node.hwtype
        if ntp == 'pc8' or ntp == 'pc48':
            rtn += 10
        elif ntp == 'pc1' or ntp == 'ixp' or ntp == 'nsp':
            rtn += 1
        else: #it's a vswitch
            for n in self.graph.neighbors(node):
                seen = False
                for elem in nodes_seen:
                    if elem == n:
                        seen = True
                        break
                if seen:
                    continue
                nodes_seen.append(n)
                rtn += self.sum_edge_links(subnet, n, nodes_seen)
        if debug_level > 2:
            print 'sum_edge_links ' + node.label + ' ' + node.hwtype + ' ' + ntp + ' rtn:' + repr(rtn)
        return rtn

    def calculate_edge_loads(self):
        #reservation subnets will be connected components in the graph since ports are not connected through larger node
        subnets = self.subnets
        edge_info_att = networkx.get_edge_attributes(self.graph, 'info')
        if debug_level > 2:
            print 'calculate_edge_loads: #subnets = ' + repr(len(subnets)) + '\n'
        for sn in subnets:
        #cycle through edges for each node in edge calculate the load generated from each side
            for e in sn.edges:      
                n1 = e[0]
                n2 = e[1]
                #print 'cel: node: ' + n.label + ' edge (' + n1.label + ', ' + n2.label + ')\n'
                if e in edge_info_att:
                    edge_info = edge_info_att[e]
                    nodes_visited = []
                    nodes_visited.append(n1)
                    nodes_visited.append(n2)
                    edge_info.rload = self.sum_edge_links(sn.nodes, n1, nodes_visited)
                    nodes_visited = []
                    nodes_visited.append(n1)
                    nodes_visited.append(n2)
                    edge_info.lload = self.sum_edge_links(sn, n2, nodes_visited)
                    edge_info.calculate_costs()
                else:
                    print "cel: edge not in edge info so we've already seen it in the opposite direction"
    def merge_vswitches(self):
        edge_info_att = networkx.get_edge_attributes(self.graph, 'info') 
        found_merge = True
        while found_merge:
            found_merge = False
            for e in edge_info_att.iteritems():
                einfo = e[1]
                if einfo.node1.hwtype == 'vswitch' and einfo.node2.hwtype == 'vswitch':
                    total_load = einfo.lload + einfo.rload
                    if total_load <= 10: 
                        #merge vswitches
                        if debug_level > 0:
                            print 'merge_vswitches merging ' + einfo.node1.label + ' and ' + einfo.node2.label + ' total load is ' + repr(total_load)
                        #merge node2 into node1
                        #change node2's outgoing edges replacing old edges with new
                        incident_edges = self.get_edges(einfo.node2)
                        for ie in incident_edges:
                            if not ie == einfo:
                                if ie.node1 == einfo.node2:
                                    ie.node1 = einfo.node1
                                else:
                                    ie.node2 = einfo.node1
                                self.graph.add_edge(ie.node1, ie.node2, ie)
                        #remove node2 and its associated edges
                        self.graph.remove_node(einfo.node2)
                        #break and restart merging
                        found_merge = True
                        break

    def calculate_node_costs(self):
        rtn = []
        self.calculate_edge_loads()
        if debug_level > 0:
            self.merge_vswitches()
        for tp in self.type_map.iteritems():
            for elem in tp[1]:
                cost = 0
                edge_info_att = networkx.get_edge_attributes(self.graph, 'info')
                if tp[0] == 'vswitch':
                    for e in self.graph.edges(elem):
                        e_info = None
                        if e in edge_info_att:
                            e_info = edge_info_att[e]
                        else:
                            #look for edge in reverse
                            tmp_e = (e[1], e[0])
                            if tmp_e in edge_info_att:
                                e_info = edge_info_att[tmp_e]
                        cost += e_info.cost
                else:
                    for n in elem.ports:
                        for e in self.graph.edges(n):
                            e_info = None
                            if e in edge_info_att:
                                e_info = edge_info_att[e]
                            else:
                                #look for edge in reverse
                                tmp_e = (e[1], e[0])
                                if tmp_e in edge_info_att:
                                    e_info = edge_info_att[tmp_e]
                            cost += e_info.cost
                elem.cost = cost
                inserted = False
                i = 0
                #print 'node(' + elem.label + ') cost: ' + repr(elem.get_cost())
                for rtn_elem in rtn:
                    if rtn_elem.get_cost() < elem.get_cost():
                        rtn.insert(i, elem)
                        inserted = True
                        break
                    i += 1
                if not inserted:
                    rtn.append(elem)
        return rtn

    def get_neighbors(self, node):
        rtn = []
        if node.hwtype == 'vswitch':
            for n in self.graph.neighbors(node):
                rtn.append(n)
        else:
            for p in node.ports:
                for n in self.graph.neighbors(p):
                    rtn.append(n)
        return rtn

    def get_leaf_neighbors(self, node): #returns leaf neighbors in order of node cost
        rtn = []
        leaves = []
        if node.hwtype == 'vswitch':
            for n in self.graph.neighbors(node):
                if not (n.hwtype == 'vswitch'):
                    leaves.append(n)
        else:
            for p in node.ports:
                for n in self.graph.neighbors(p):
                    if not (n.hwtype == 'vswitch'):
                        leaves.append(n)
        for n in leaves:
            inserted = False
            i = 0
            for elem in rtn:
                if n.cost > elem.cost:
                    rtn.insert(i, n) 
                    inserted = True
                    break
                i += 1
            if not inserted:
                rtn.append(n)
        return rtn

    def print_mapping(self):
        print 'mapping of user graph:' + self.label + ' file:' + self.file
        for tp in self.type_map.iteritems():
            for n in tp[1]:
                if n.mapped_to == None:
                    print ' user node:' + n.label + ' mapped to: None'
                else:
                    print ' user node:' + n.label + ' mapped to:' + n.mapped_to.label 
        edge_att = networkx.get_edge_attributes(self.graph, 'info')
        self.ice_cost = 0
        for e in edge_att.iteritems():
            e[1].calculate_intercluster_cost()
            self.ice_cost += e[1].intercluster_cost
            e[1].print_mapping()
        print ' METRIC: user graph(' + self.label + ',' + self.file + ') INTERCLUSTER_COST(' + repr(self.ice_cost) + ') HOST_BW(' + repr(2*self.host_cost) + ') \n'   

    def calculate_metric(self): 
        edge_att = networkx.get_edge_attributes(self.graph, 'info')
        self.ice_cost = 0
        for e in edge_att.iteritems():
            e[1].calculate_intercluster_cost()
            self.ice_cost += e[1].intercluster_cost
 

    def get_edges(self, node): #return edges incident to node
        #return info
        rtn = []
        edge_att = networkx.get_edge_attributes(self.graph, 'info')
        if (node.hwtype == 'vswitch') or node.parent:
            for e in self.graph.edges(node):
                if e in edge_att:
                    rtn.append(edge_att[e])
                else:
                    tmp_e = (e[1], e[0])
                    if tmp_e in edge_att:
                        rtn.append(edge_att[tmp_e])
                    else:
                        print 'get_edges: cannot find einfo for (' + e[0].label + ',' + e[1].label + ')'
        else:
            for port in node.ports:
                for e in self.graph.edges(port):
                    if e in edge_att:
                        rtn.append(edge_att[e])
                    else:
                        tmp_e = (e[1], e[0])
                        if tmp_e in edge_att:
                            rtn.append(edge_att[tmp_e])
                        else:
                            print 'get_edges: cannot find einfo for (' + e[0].label + ',' + e[1].label + ')'
        return rtn

    def get_mapped_edges(self, node): #return edges incident to node with one mapped endpoint other than itself
        mapped_edges = []
        for e in self.get_edges(node):
            if (e.node2 == node or e.node2.parent == node) and e.node1.mapped_to:
                mapped_edges.append(e)
            elif (e.node1 == node or e.node1.parent == node) and e.node2.mapped_to:
                mapped_edges.append(e)
        return mapped_edges

    def get_new_vswitch(self):
        ndx = 1
        if 'vswitch' in self.type_map:
            for v in self.type_map['vswitch']:
                if v.index >= ndx:
                    ndx = v.index + 1 
        nlabel = 'vswitch.' + repr(ndx)
        tmp_node = self.get_node(nlabel, 'vswitch')
        tmp_node.index = ndx
        self.graph.add_node(tmp_node)
        return tmp_node

#break nodes into types
class Cluster:
    def __init__(self, sw, swtype):
       self.sw = sw
       self.swtype = swtype
       self.hw = {}
       self.hw[swtype] = []
       self.hw[swtype].append(self.sw)
       self.graph = None #networkx.Graph()
       #self.graph.add_node(sw)
    def add_component(self, node): #, cap=None):
       if not (node.hwtype in self.hw):
           self.hw[node.hwtype] = []
       self.hw[node.hwtype].append(node)
       #self.graph.add_node(node)
       #c = cap
       #if cap == None:
       #    c = self.swtype
       #edge = Edge(node, self.sw, c)
       #self.graph.add_edge(node, self.sw, info=edge)
    def get_graph(self):
        if self.graph == None:
            self.graph = networkx.subgraph(testbed_viz, self.hw)
        return self.graph
    def is_available(self, hwtype):
        r = self.get_available(hwtype)
        if r:
            return True
        return False
    def get_available(self, hwtype):
        if hwtype == 'vswitch':
            return self.sw
        if not (hwtype in self.hw):
            return None
        for node in self.hw[hwtype]:
            if node.res_id == 'free':
                return node
        return None
    def contains_component(self, node):
        if (node.hwtype in self.hw) and (node in self.hw[node.hwtype]):
            return True
        elif node == self.sw:
            return True
        else:
            return False
    def not_used(self, rid = None):
        for hw in self.hw.iteritems():
            for n in hw[1]:
                if rid:
                    if n.res_id == rid:
                        return False
                elif not (n.res_id == 'free'):
                    return False
        return True 

clusters = []

for i in range(11):
    clusters.append(None)

pc8_ndx = 1
pc48_ndx = 1
sw1s = []
sw10s = []
pc8cores = []
pc48cores = []

#create clusters for 10Gb switches
for i in range(7, 11):
    swlabel = 'sw' + repr(i)
    if debug_level > 2:
        print swlabel
    sw = Node(swlabel, 'sw10')
    sw10s.append(sw)
    cluster = Cluster(sw, 10)
    #testbed.add_node(cluster)
    clusters[i] = cluster
    #add pc8cores
    num_pc8s = 8
    if i > 8:
        num_pc8s = 6
    for j in range(num_pc8s):
        nlabel = 'pc8core' + repr(pc8_ndx)
        pc8 = Node(nlabel, 'pc8')
        pc8.index = pc8_ndx
        cluster.add_component(pc8)
        edge = Edge(pc8, cluster.sw, 10)
        testbed_viz.add_edge(edge.node1, edge.node2, info=edge)
        port = Node((nlabel + '.0'), 'pc8')
        port.parent = pc8
        pc8.ports.append(port)
        edge = Edge(port, cluster.sw, 10)
        testbed.add_edge(edge.node1, edge.node2, info=edge)
        #edge = Edge(cluster.sw, port, 10)
        #testbed.add_edge(edge.node1, edge.node2, info=edge)
        pc8cores.append(pc8)
        pc8_ndx += 1

    #add pc48cores
    if i < 9:
        nlabel = 'pc48core' + repr(pc48_ndx)
        pc48 = Node(nlabel, 'pc48')
        pc48.index = pc48_ndx
        cluster.add_component(pc48)
        edge = Edge(pc48, cluster.sw, 20)
        testbed_viz.add_edge(edge.node1, edge.node2, info=edge)
        port = Node((nlabel + '.0'), 'pc48')
        port.parent = pc48
        pc48.ports.append(port)
        edge = Edge(port, cluster.sw, 10)
        testbed.add_edge(edge.node1, edge.node2, info=edge)
        #edge = Edge(cluster.sw, port, 10)
        #testbed.add_edge(edge.node1, edge.node2, info=edge)
        port = Node((nlabel + '.1'), 'pc48')
        port.parent = pc48
        pc48.ports.append(port)
        edge = Edge(port, cluster.sw, 10)
        testbed.add_edge(edge.node1, edge.node2, info=edge)
        #edge = Edge(cluster.sw, port, 10)
        #testbed.add_edge(edge.node1, edge.node2, info=edge)
        pc48cores.append(pc48)
        pc48_ndx += 1


#add edges between 10Gb switches
for i in range(7, 11):
    nextc = None
    if i == 10:
        nextc = clusters[7]
    else:
        nextc = clusters[i+1]
    edge = InterClusterEdge(clusters[i].sw, nextc.sw, 10)
    testbed_viz.add_edge(edge.node1, edge.node2, info=edge)
    edge = InterClusterEdge(clusters[i].sw, nextc.sw, 10)
    testbed.add_edge(edge.node1, edge.node2, info=edge)
    #edge = Edge(nextc.sw, clusters[i].sw, 10)
    #testbed.add_edge(edge.node1, edge.node2, info=edge)
    edge = InterClusterEdge(clusters[i].sw, nextc.sw, 10)
    edge.id = 1
    testbed_viz.add_edge(edge.node1, edge.node2, info=edge)
    edge = InterClusterEdge(clusters[i].sw, nextc.sw, 10)
    edge.id = 1
    testbed.add_edge(edge.node1, edge.node2, info=edge)
    #edge = Edge(nextc.sw, clusters[i].sw, 10)
    #edge.id = 1
    #testbed.add_edge(edge.node1, edge.node2, info=edge)


pc1_ndx = 1
ixp_ndx = 1
nsp_ndx = 1
pc1cores = []
ixps = []
nsps = []
for i in range(1,7):
    swlabel = 'sw' + repr(i)
    if debug_level > 2:
        print swlabel
    sw = Node(swlabel, 'sw1')
    sw1s.append(sw)
    cluster = Cluster(sw, 1)
    #testbed.add_node(cluster)
    clusters[i] = cluster
    #add pc1cores
    num_pc1s = 12
    if i == 5:
        num_pc1s = 14
    elif i == 6:
        num_pc1s = 10
    for j in range(num_pc1s):
        nlabel = 'pc1core' + repr(pc1_ndx)
        pc1 = Node(nlabel, 'pc1')
        pc1.index = pc1_ndx
        cluster.add_component(pc1)
        edge = Edge(pc1, cluster.sw, 1)
        testbed_viz.add_edge(edge.node1, edge.node2, info=edge)
        port = Node((nlabel + '.0'), 'pc1')
        port.parent = pc1
        pc1.ports.append(port)
        edge = Edge(port, cluster.sw, 1)
        testbed.add_edge(edge.node1, edge.node2, info=edge)
        #edge = Edge(cluster.sw, port, 1)
        #testbed.add_edge(edge.node1, edge.node2, info=edge)
        pc1cores.append(pc1)
        pc1_ndx += 1
    #add ixp
    num_ixp = 1 #2
    if i == 6:
        num_ixp = 2 #4
    for j in range(num_ixp):
        nlabel = 'ixp' + repr(ixp_ndx)
        ixp = Node(nlabel, 'ixp')
        ixp.index = ixp_ndx
        cluster.add_component(ixp)
        edge = Edge(ixp, cluster.sw, 1)
        testbed_viz.add_edge(edge.node1, edge.node2, info=edge)
        #add ixp ports
        for k in range(10):
            port = Node((nlabel + '.' + repr(k)), 'ixp')
            port.parent = ixp
            ixp.ports.append(port)
            edge = Edge(port, cluster.sw, 1)
            testbed.add_edge(edge.node1, edge.node2, info=edge)
            #edge = Edge(cluster.sw, port, 1)
            #testbed.add_edge(edge.node1, edge.node2, info=edge)
        ixps.append(ixp)
        ixp_ndx += 2
    
    #add nsp
    if i < 5:
        nlabel = 'nsp' + repr(nsp_ndx)
        nsp = Node(nlabel, 'nsp')
        nsp.index = nsp_ndx
        cluster.add_component(nsp) 
        edge = Edge(nsp, cluster.sw, 1)
        testbed_viz.add_edge(edge.node1, edge.node2, info=edge)
        #add nsp ports
        for k in range(8):
            port = Node((nlabel + '.' + repr(k)), 'nsp')
            port.parent = nsp
            nsp.ports.append(port)
            edge = Edge(port, cluster.sw, 1)
            testbed.add_edge(edge.node1, edge.node2, info=edge)
            #edge = Edge(cluster.sw, port, 1)
            #testbed.add_edge(edge.node1, edge.node2, info=edge)
        nsps.append(nsp)
        nsp_ndx += 1
    
    #add inter cluster edges
    for j in range(7, 11):
        edge = InterClusterEdge(cluster.sw, clusters[j].sw, 10)
        testbed_viz.add_edge(edge.node1, edge.node2, info=edge)
        edge = InterClusterEdge(cluster.sw, clusters[j].sw, 10)
        testbed.add_edge(edge.node1, edge.node2, info=edge)
        #edge = Edge(clusters[j].sw, cluster.sw, 10)
        #testbed.add_edge(edge.node1, edge.node2, info=edge)


testbed_typemap = {'sw1':sw1s, 'sw10':sw10s, 'pc8':pc8cores, 'pc48':pc48cores, 'pc1':pc1cores, 'ixp':ixps, 'nsp':nsps}


def is_available(typemap):
    for tp in testbed_typemap.iteritems():
        num_available = 0
        for n in tp[1]:
            if n.res_id == 'free':
                num_available += 1
        if (tp[0] in typemap) and (num_available < len(typemap[tp[0]])):
            return False
    return True
        
def map_edges(unode, ugraph, rnode):
    mapped_edges = ugraph.get_mapped_edges(unode)
    testbed_edge_att = networkx.get_edge_attributes(testbed, 'info')
    if debug_level > 2: # or unode.hwtype == 'ixp':
        print 'map ' + unode.label + ' edges:' + repr(len(mapped_edges))
    for e in mapped_edges:
        if debug_level > 2:
            print '  edge:' + e.node1.label + ', ' + e.node2.label
        if e.is_mapped(): #skip this edge if it is already mapped to a path
            continue
        source = None
        sink = None
        if e.node1.parent == unode or e.node1 == unode:
            if unode.hwtype == 'vswitch':
                source = rnode
            else:
                source = rnode.ports[e.node1.index]
            sink = e.node2.mapped_to
        else:
            source = e.node1.mapped_to
            if unode.hwtype == 'vswitch':
                sink = rnode
            else:
                sink = rnode.ports[e.node2.index]
        found_path = find_cheapest_path(source, sink, e)
        if found_path:
            if debug_level > 0:
                path_str = ''
                for k in range(len(found_path[0])):
                    path_str = path_str + found_path[0][k].label + ', '
                print '    me:found path for (' + e.node1.label + ', ' + e.node2.label + ') mapped (' + source.label + ',' + sink.label + ') path:(' + path_str + ')'
            path = found_path[0]
            path_length =len(path) - 1
            i = 0         
            erload = e.rload
            if erload > 10:
                erload = 10
            elload = e.lload
            if elload > 10:
                elload = 10
            while i < path_length:
                rpe = (path[i], path[i+1])
                is_right = True
                if not (rpe in testbed_edge_att):
                    rpe = (path[i+1], path[i])
                    is_right = False
                e_info = testbed_edge_att[rpe]
                #adjust testbed link's load in both directions 
                if e_info.inter_cluster:
                    if is_right:
                        e_info.rload += erload
                        e_info.lload += elload
                    else:
                        e_info.rload += elload
                        e_info.lload += erload
                #add edge to path
                e.path.add_edge(e_info)
                i += 1

def map_node(unode, ugraph, cluster, mapping_neighbor=False):
    #map unode and it's ports
    rnode = None
    first_map = True
    if unode.mapped_to:
         rnode = unode.mapped_to
         first_map = False
    else:
        rnode = cluster.get_available(unode.hwtype)
        unode.map_to(rnode)
    if debug_level > 0:
        if first_map:
            print 'map ' + unode.label + ' to cluster ' + cluster.sw.label + ' physical node:' + rnode.label
        else:
            print 'finish map ' + unode.label + ' to cluster ' + cluster.sw.label + ' physical node:' + rnode.label
    #neighbors = ugraph.get_leaf_neighbors(unode)
    #for n in neighbors:
        #if neighbor is a leaf i.e. not a vswitch and it's not already mapped, try to map it
        #if n.mapped_to == None :
            #anode = cluster.get_available(n.hwtype)
            #if anode:
                #print ' map neighbor ' + n.parent.label + ' to physical node:' + anode.label
                #n.map_to(anode.ports[n.index])
    #map any edges
    map_edges(unode, ugraph, rnode)
    #if mapping_neighbor: #we're mapping someone's neighbor and we don't want to map their neighbors we want to save it for later
     #   return
    neighbors = ugraph.get_leaf_neighbors(unode)
    unmapped_nodes = []
    for n in neighbors:
        #if neighbor is a leaf i.e. not a vswitch and it's not already mapped, try to map it
        if n.mapped_to == None :#and n.parent:
            nmedges = ugraph.get_mapped_edges(n.parent)
            mcost = compute_mapping_cost(cluster, n.parent, ugraph, nmedges)
            if mcost >= 0:
                 anode = cluster.get_available(n.hwtype)
                 if anode:
                     if debug_level > 0:
                         print ' map neighbor ' + n.parent.label + ' to physical node:' + anode.label
                     n.map_to(anode.ports[n.index])
                 else:
                     unmapped_nodes.append(n)
            else:
                unmapped_nodes.append(n)
    map_edges(unode, ugraph, rnode) #go through and map any mapped edges again this may have changed with neighbor mappings
    
    #if this is a vswitch and we have more than 1 unmapped leaf split the node and insert a new node into the graph. return the new node
    #need to make sure the unmapped nodes do not contribute a load that exceeds the bandwidth of the interswitch edge
    if unode.hwtype == 'vswitch' and len(unmapped_nodes) > 1 and split_vswitches:
        rload = 0
        lload = 0
        for elem in ugraph.get_edges(unode):
            if elem.node1 == unode:
                if elem.node2 not in unmapped_nodes:
                    rload += elem.lload
                else:
                    lload += elem.rload
            else:
                if elem.node1 not in unmapped_nodes:
                    rload += elem.rload
                else:
                    lload += elem.lload
        if rload > 10 or lload > 10:#can't support this split instead may have to split into several switches that can support the load
            return None
        rload = 0
        lload = 0
        new_vswitch = ugraph.get_new_vswitch()
        ncost = 0
        removal_str = ''
        addition_str = ''
        for elem in ugraph.get_edges(unode):
            if elem.node1 == unode:
                if elem.node2 not in unmapped_nodes:
                    rload += elem.lload
                else:
                    lload += elem.rload
                    ugraph.graph.remove_edge(elem.node1, elem.node2)
                    removal_str = removal_str + ' (' + elem.node1.label + ',' + elem.node2.label + ')'
                    elem.node1 = new_vswitch
                    ugraph.graph.add_edge(elem.node1, elem.node2, info=elem)
                    addition_str = addition_str + ' (' + elem.node1.label + ',' + elem.node2.label + ')'
                    ncost =+ elem.cost
            else:
                if elem.node1 not in unmapped_nodes:
                    rload += elem.rload
                else:
                    lload += elem.lload
                    ugraph.graph.remove_edge(elem.node1, elem.node2)
                    removal_str = removal_str + ' (' + elem.node1.label + ',' + elem.node2.label + ')'
                    elem.node2 = new_vswitch
                    ugraph.graph.add_edge(elem.node1, elem.node2, info=elem)
                    addition_str = addition_str + ' (' + elem.node1.label + ',' + elem.node2.label + ')'
                    ncost =+ elem.cost

        ie = ReservationEdge(unode, new_vswitch, 10)
        ie.lload = lload
        ie.rload = rload
        ie.calculate_costs()
        unode.cost = unode.cost - ncost + ie.cost
        new_vswitch.cost = ncost + ie.cost
        ugraph.graph.add_edge(unode, new_vswitch, info=ie)
        addition_str = addition_str + ' (' + unode.label + ',' + new_vswitch.label + ')'
        print '   mn: splitting vswitch ' + unode.label + ' adding node ' + new_vswitch.label 
        print '     adding edges:' + addition_str
        print '     removing edges:' + removal_str
        ugraph.recompute_subnets()
        return new_vswitch

    return None

def map_node_split(unode, ugraph, cluster, mapping_neighbor=False):
    #map unode and it's ports
    rnode = None
    first_map = True
    if unode.mapped_to:
         rnode = unode.mapped_to
         first_map = False
    else:
        rnode = cluster.get_available(unode.hwtype)
        unode.map_to(rnode)
    if debug_level > 0:
        if first_map:
            print 'map ' + unode.label + ' to cluster ' + cluster.sw.label + ' physical node:' + rnode.label
        else:
            print 'finish map ' + unode.label + ' to cluster ' + cluster.sw.label + ' physical node:' + rnode.label
    #neighbors = ugraph.get_leaf_neighbors(unode)
    #for n in neighbors:
        #if neighbor is a leaf i.e. not a vswitch and it's not already mapped, try to map it
        #if n.mapped_to == None :
            #anode = cluster.get_available(n.hwtype)
            #if anode:
                #print ' map neighbor ' + n.parent.label + ' to physical node:' + anode.label
                #n.map_to(anode.ports[n.index])
    #map any edges
    map_edges(unode, ugraph, rnode)
    #if mapping_neighbor: #we're mapping someone's neighbor and we don't want to map their neighbors we want to save it for later
     #   return
    neighbors = ugraph.get_leaf_neighbors(unode)
    unmapped_nodes = []
    for n in neighbors:
        #if neighbor is a leaf i.e. not a vswitch and it's not already mapped, try to map it
        if n.mapped_to == None :#and n.parent:
            nmedges = ugraph.get_mapped_edges(n.parent)
            mcost = compute_mapping_cost(cluster, n.parent, ugraph, nmedges)
            if mcost >= 0:
                 anode = cluster.get_available(n.hwtype)
                 if anode:
                     if debug_level > 0:
                         print ' map neighbor ' + n.parent.label + ' to physical node:' + anode.label
                     n.map_to(anode.ports[n.index])
                 else:
                     unmapped_nodes.append(n)
            else:
                unmapped_nodes.append(n)
    map_edges(unode, ugraph, rnode) #go through and map any mapped edges again this may have changed with neighbor mappings
    
    #if this is a vswitch and we have more than 1 unmapped leaf split the node and insert a new node into the graph. return the new node
    #need to make sure the unmapped nodes do not contribute a load that exceeds the bandwidth of the interswitch edge
    if unode.hwtype == 'vswitch' and len(unmapped_nodes) > 1 and split_vswitches:
        rload = 0
        lload = 0
        for elem in ugraph.get_edges(unode):#look through mapped edges
            if elem.node1 == unode:
                if elem.node2 not in unmapped_nodes:
                    rload += elem.lload
            elif elem.node1 not in unmapped_nodes:
                    rload += elem.rload
        if rload > 10 :#can't support this split load on new edge is already maxed
            return None

        new_vswitch = ugraph.get_new_vswitch()
        ncost = 0
        removal_str = ''
        addition_str = ''
        for elem in ugraph.get_edges(unode):
            if elem.node1 == unode:
                if elem.node2 not in unmapped_nodes:
                    rload += elem.lload
                else:
                    lload += elem.rload
                    ugraph.graph.remove_edge(elem.node1, elem.node2)
                    removal_str = removal_str + ' (' + elem.node1.label + ',' + elem.node2.label + ')'
                    elem.node1 = new_vswitch
                    ugraph.graph.add_edge(elem.node1, elem.node2, info=elem)
                    addition_str = addition_str + ' (' + elem.node1.label + ',' + elem.node2.label + ')'
                    ncost =+ elem.cost
            else:
                if elem.node1 not in unmapped_nodes:
                    rload += elem.rload
                else:
                    lload += elem.lload
                    ugraph.graph.remove_edge(elem.node1, elem.node2)
                    removal_str = removal_str + ' (' + elem.node1.label + ',' + elem.node2.label + ')'
                    elem.node2 = new_vswitch
                    ugraph.graph.add_edge(elem.node1, elem.node2, info=elem)
                    addition_str = addition_str + ' (' + elem.node1.label + ',' + elem.node2.label + ')'
                    ncost =+ elem.cost

        ie = ReservationEdge(unode, new_vswitch, 10)
        ie.lload = lload
        ie.rload = rload
        ie.calculate_costs()
        unode.cost = unode.cost - ncost + ie.cost
        new_vswitch.cost = ncost + ie.cost
        ugraph.graph.add_edge(unode, new_vswitch, info=ie)
        addition_str = addition_str + ' (' + unode.label + ',' + new_vswitch.label + ')'
        print '   mn: splitting vswitch ' + unode.label + ' adding node ' + new_vswitch.label 
        print '     adding edges:' + addition_str
        print '     removing edges:' + removal_str
        ugraph.recompute_subnets()
        return new_vswitch

    return None
    
#find the cheapest path between the source and the sink returns a tuple (path, num_intercluster_links)
class NodePath:
    def __init__(self, path = None):
        self.nodes = []
        self.cost = 0
        if path:
            for i in range(path.length()):
                self.nodes.append(path.nodes[i])
    def get_last(self):
        return self.nodes[len(self.nodes) - 1]
    def length(self):
        return len(self.nodes)

def find_cheapest_path(source, sink, ueinfo, potential_edges = None):
    #debug = False
    #if source.hwtype == 'sw1' and sink.hwtype == 'sw1':
     #   debug = False
    #if source.hwtype == 'ixp' or sink.hwtype == 'ixp':
     #   debug = True
    #if source.hwtype == 'ixp' and sink.hwtype == 'ixp':
        #print 'find_cheapest_path ' + source.label + ' ' + sink.label
    urload = ueinfo.rload
    if urload > 10:
        urload = 10
    ulload = ueinfo.lload
    if ulload > 10:
        ulload = 10
    current_paths = []
    path = NodePath()
    path.nodes.append(source)
    current_paths.append(path)
    testbed_edge_att = networkx.get_edge_attributes(testbed, 'info')


    while len(current_paths) > 0:
        new_paths = []
        for path in current_paths:
            last_node = path.get_last()
            neighbors = testbed.neighbors(last_node)
            for n in neighbors:
                new_path = NodePath(path)
                
                new_path.nodes.append(n)
                
                if n == sink:
                    good_path = True
                    path_length = new_path.length() - 2
                    path_cost = 0
                    i = 0
                    if debug_level > 3:# or (potential_edges == None and source.hwtype == 'ixp' and sink.hwtype == 'ixp'):
                        path_str = ''
                        for k in range(new_path.length()):
                            path_str = path_str + new_path.nodes[k].label + ', '
                        print '    path:(' + path_str + ')'
                    while i < path_length:
                        rpe = (path.nodes[i], path.nodes[i+1])
                        is_right = True
                        if not (rpe in testbed_edge_att):
                            rpe = (path.nodes[i+1], path.nodes[i])
                            is_right = False
                        e_info = testbed_edge_att[rpe]
                        if e_info.inter_cluster:
                            if is_right:
                                if potential_edges and (rpe in potential_edges):
                                    if (potential_edges[rpe][0] < urload) or (potential_edges[rpe][1] < ulload):
                                        if debug_level > 3:
                                            print '    potential_edge is overloaded:(' + rpe[0].label + ', ' + rpe[1].label + ') unallocated_bw(' +  repr(potential_edges[rpe][0]) + ',' + repr(potential_edges[rpe][1]) + ') uedge load:(' + repr(ueinfo.rload) + ',' + repr(ueinfo.lload) + ')' 
                                        good_path = False
                                elif (e_info.unallocated_right_bw() < urload) or (e_info.unallocated_left_bw() < ulload):
                                    if debug_level > 3:
                                        print '    edge is overloaded:(' + e_info.node1.label + ', ' + e_info.node2.label + ') unallocated_bw(' +  repr(e_info.unallocated_right_bw()) + ',' + repr(e_info.unallocated_left_bw()) + ') uedge load:(' + repr(ueinfo.rload) + ',' + repr(ueinfo.lload) + ')'
                                    good_path = False
                            else:
                                if potential_edges and (rpe in potential_edges):
                                    if (potential_edges[rpe][0] < ulload) or (potential_edges[rpe][1] < urload):
                                        if debug_level > 3:
                                            print '    potential_edge is overloaded:(' + rpe[0].label + ', ' + rpe[1].label + ') unallocated_bw(' +  repr(potential_edges[rpe][0]) + ',' + repr(potential_edges[rpe][1]) + ') uedge load:(' + repr(ueinfo.rload) + ',' + repr(ueinfo.lload) + ')' 
                                        good_path = False
                                elif (e_info.unallocated_right_bw() < ulload) or (e_info.unallocated_left_bw() < urload):
                                    if debug_level > 3:
                                        print '    edge is overloaded:(' + e_info.node1.label + ', ' + e_info.node2.label + ') unallocated_bw(' +  repr(e_info.unallocated_right_bw()) + ',' + repr(e_info.unallocated_left_bw()) + ') uedge load:(' + repr(ueinfo.rload) + ',' + repr(ueinfo.lload) + ')'
                                    good_path = False
                            if not good_path:
                                break
                            new_path.cost += 1
                        i += 1
                    if good_path:
                        if debug_level > 2:#source.hwtype == 'ixp' and sink.hwtype == 'ixp':
                            path_str = ''
                            for k in range(new_path.length()):
                                path_str = path_str + new_path.nodes[k].label + ', '
                                print '    good path:(' + path_str + ')'
                        return (new_path.nodes, new_path.cost)
                elif (n not in path.nodes) and (not n.is_leaf()):
                    new_paths.append(new_path)
        current_paths = new_paths
    return None

#calculate mapping cost for cluster returns -1 if mapping not feasible and cost otw
def compute_mapping_cost(cluster, unode, ugraph, mapped_edges):
    cluster_cost = 0
    n = cluster.get_available(unode.hwtype)
    testbed_edge_att = networkx.get_edge_attributes(testbed, 'info')
    if not n:
        if debug_level > 1:
            print 'compute_mapping_cost  node:' + unode.label + ' cluster:' + cluster.sw.label + ' --type not available'
        return -1
    if cluster.not_used():#cluster not allocated to any user graph
        cluster_cost += 50
    elif cluster.not_used(ugraph.label): #cluster not allocated to any node in ugraph
        cluster_cost += 20
    if unode.hwtype == 'vswitch':
        #check if cluster already contains either an edge or a switch for this subnet
        subnet = ugraph.get_subnet(unode)
        #first check if a vswitch is already mapped to this cluster from the subnet
        if debug_level > 3:
            print 'compute_mapping_cost  node:' + unode.label + ' cluster:' + cluster.sw.label + ' subnet nodes'
        for snode in subnet.nodes:
            if debug_level > 3:
                if snode.mapped_to:
                    print '    ' + snode.label + ' mapped_to:' + snode.mapped_to.label
                else:
                    print '    ' + snode.label + ' mapped_to:None'
            if snode.mapped_to and (snode.hwtype == 'vswitch') and cluster.contains_component(snode.mapped_to):
                if debug_level > 2:
                    print 'compute_mapping_cost  node:' + unode.label + ' cluster:' + cluster.sw.label + ' --subnet vswitch:' + snode.label + ' already mapped here'
                return -1
        #next check if a subnet link has been mapped to this cluster
        edge_info_att = networkx.get_edge_attributes(ugraph.graph, 'info')
        for e in subnet.edges:
            e_info = None
            if e in edge_info_att:
                e_info = edge_info_att[e]
            else:
                #e2 = (e[1],e[0])
                #if e2 in edge_info_att:
                #    e_info = edge_info_att[e2]
                #else:
                print '   cmc error e(' + e[0].label + ',' + e[1].label + ') not in attributes'
            for pe in e_info.path.edges:
                if pe.inter_cluster and (cluster.contains_component(pe.node1) or cluster.contains_component(pe.node2)):
                        if debug_level > 1:
                            print 'compute_mapping_cost  node:' + unode.label + ' cluster:' + cluster.sw.label + ' --subnet vswitch:' + snode.label + ' already mapped here'
                        return -1

    potential_edges = {}
    #look through all the edges with a mapped node and make sure there is a feasible path
    for e in mapped_edges:
        if e.is_mapped(): #if the edge already has a path skip it
            continue
        source = None
        sink = None
        if (e.node1.parent == unode) or (e.node1 == unode):
            if unode.hwtype == 'vswitch':
                source = n
            else:
                source = n.ports[e.node1.index]
            sink = e.node2.mapped_to
        else:
            source = e.node1.mapped_to
            if unode.hwtype == 'vswitch':
                sink = n
            else:
                sink = n.ports[e.node2.index]
        #e_load = e.load;
        #if unode.hwtype == 'ixp':
        found_path = find_cheapest_path(source, sink, e, potential_edges)
        if found_path and debug_level > 3:
            path_str = ''
            for k in range(len(found_path[0])):
                path_str = path_str + found_path[0][k].label + ', '
            print '    cmc:found path for (' + e.node1.label + ', ' + e.node2.label + ') mapped (' + source.label + ',' + sink.label + ') path:(' + path_str + ')'
        if not found_path:
            print 'compute_mapping_cost  node:' + unode.label + ' cluster:' + cluster.sw.label + ' --could not find path for mapped edge(' + source.label + ',' + sink.label + ') uedge('  + e.node1.label + ',' + e.node2.label + ')'
            return -1
        else:
            #found the cheapest path for this edge 
            #add path cost to the total cluster cost for mapping this node
            cluster_cost += (found_path[1] * e.cost)
            #add this path to potential edges so we can use the calculations for the rest of the edges
            path = found_path[0]
            path_length =len(path) - 1
            i = 0         
            while i < path_length:
                rpe = (path[i], path[i+1])
                is_right = True
                if not (rpe in testbed_edge_att):
                    rpe = (path[i+1], path[i])
                    is_right = False
                e_info = testbed_edge_att[rpe]
                rbw = 0
                lbw = 0
                #for pe in path:
                if e_info.inter_cluster:
                    erload = e.rload
                    if erload > 10:
                        erload = 10
                    elload = e.lload
                    if elload > 10:
                        elload = 10
                    if potential_edges and (rpe in potential_edges):
                        if is_right:
                            rbw = potential_edges[rpe][0] - erload
                            lbw = potential_edges[rpe][1] - elload
                        else:
                            rbw = potential_edges[rpe][0] - elload
                            lbw = potential_edges[rpe][1] - erload
                    else:
                        if is_right:
                            rbw = e_info.unallocated_right_bw() - erload
                            lbw = e_info.unallocated_left_bw() - elload
                        else:
                            rbw = e_info.unallocated_right_bw() - elload
                            lbw = e_info.unallocated_left_bw() - erload                        
                    potential_edges[rpe] = (rbw,lbw)
                i += 1
    #look at the potential cost of the neighbor nodes we can't map to this cluster  
    #order unmapped leaf neighbors by cost try and map highest cost first
    neighbor_edges = ugraph.get_edges(unode)
    lneighbors = ugraph.get_leaf_neighbors(unode) #returns a list of leaf neighbors ordered by cost
    potential_nodes = []
    seen_unodes = []
    potential_nodes.append(n)
    unmapped_nodes = []
    leaf_nodes = []
    for un in lneighbors:
        #if neighbor is a leaf i.e. not a vswitch and it's not already mapped, try to map it
        if un and (un.mapped_to == None) and (un not in seen_unodes): #if a neighbor node can not be mapped add the cost of its link. if nn is None then it's a vswitch
            can_map = False
            if un.hwtype in cluster.hw:
                for cn in cluster.hw[un.hwtype]:
                    if (cn.res_id == 'free') and (not(cn in potential_nodes)):
                        potential_nodes.append(cn)
                        can_map = True
                        break
            if not can_map:#we can't map this then find the edge cost for mapping this to another cluster
                unmapped_nodes.append(un)
                for e in neighbor_edges:
                    if e.node1.parent == un or e.node1 == un or e.node2.parent == un or e.node2 == un:
                        cluster_cost += e.cost
        seen_unodes.append(un)
    if unode.hwtype == 'vswitch' and len(potential_nodes) == 1 and len(lneighbors) > 0: #if we can't map any of the leaves don't bother mapping the vswitch
        return -1
    #if we're mapping a vswitch and have more than one unmapped nodes
    #calculate the cost of splitting this switch with everything leftover placed on a single vswitch
    if unode.hwtype == 'vswitch' and len(unmapped_nodes) > 1: #and debug_level > 0:
        #see if there is a cluster that we can map the split switch to with all of its leaves 
        mappable_cluster = None
        rload = 0
        lload = 0

        for elem in ugraph.get_edges(unode):
            if elem.node1 == unode:
                if elem.node2 not in unmapped_nodes:
                    rload += elem.lload
                else:
                    lload += elem.rload
            else:
                if elem.node1 not in unmapped_nodes:
                    rload += elem.rload
                else:
                    lload += elem.lload
        tmp_node = ReservationNode('vswitch.tmp', 'vswitch')
        ie = ReservationEdge(unode, tmp_node, 10)
        ie.lload = lload
        ie.rload = rload
        ie.calculate_costs()

        for c in clusters:
            if c == cluster or c == None:
                continue
            cpotential_nodes = []
            can_map_cluster = True
            for un in unmapped_nodes:
                if un and (un.mapped_to == None):
                    can_map = False
                    if un.hwtype in c.hw:
                        for cn in c.hw[un.hwtype]:
                            if (cn.res_id == 'free') and (not(cn in cpotential_nodes)):
                                cpotential_nodes.append(cn)
                                can_map = True
                                break
                    if not can_map:#we can't map this then find the edge cost for mapping this to another cluster
                        can_map_cluster = False
                        break
            if can_map_cluster:
                #create a temporary split in the vswitch to calculate the cost of the split
                found_path = find_cheapest_path(cluster.sw, c.sw, ie, potential_edges)
                if found_path:
                    path = found_path[0]
                    path_length =len(path) - 1
                    pcost = 0
                    i = 0         
                    while i < path_length:
                        if path[i].hwtype.startswith('sw') and path[i+1].hwtype.startswith('sw'):
                            pcost += ie.cost
                        i += 1
                    if mappable_cluster == None or mappable_cluster[1] > pcost:
                        mappable_cluster = (c, pcost)
        if mappable_cluster:
            cluster_cost += pcost   
            if debug_level > 1:
                print '  cmc: adding vswitch split cost of ' + repr(pcost)
        else: #We can't map the split vswitch so let's impose a penalty
            cluster_cost += 50                   
    return cluster_cost



def find_feasible_cluster(node, ugraph):
    rtn_cluster = None
    current_cost = 0
    #create a list of mapped nodes connected to the node we're trying to map
    mapped_edges = ugraph.get_mapped_edges(node)
    if debug_level > 3:
        print 'find_feasible_cluster ' + node.label + ' edges:' + repr(len(mapped_edges))
    #print 'find_feasible_cluster for node:' + node.label
    for c in clusters:
        #is there a component of the right type available
        #cluster_mapped = True
        if c == None:
            continue
        if node.mapped_to: #if this node is already mapped just find the cluster it belongs to and return
            if c.contains_component(node.mapped_to):
                return c
            else:
                continue
        cluster_cost = compute_mapping_cost(c, node, ugraph, mapped_edges)
        if debug_level > 1:
            print 'find_feasible_cluster: node:' + node.label + ' cluster:' + c.sw.label + ' cost:' + repr(cluster_cost)
        if cluster_cost >= 0:
            if rtn_cluster == None or cluster_cost < current_cost:
                rtn_cluster = c
                current_cost = cluster_cost
    return rtn_cluster


rcolors = ['r','b','pink','skyblue','salmon','y','g','purple','orange']

#read in user graph
user_graphs = None
if args.f:
    user_graphs = []
    i = 0
    for f in args.f:
        rid = rcolors[i]
        ugraph = Reservation(rid, f, rid)
        user_graphs.append(ugraph)
        i += 1


if user_graphs:
    removals = []
    total_ice_cost = 0
    total_host_cost = 0
    for user_graph in user_graphs:
        failed = None
        if is_available(user_graph.type_map):
            #user_graph.calculateEdgeLoads()
            node_list = user_graph.calculate_node_costs()
            if debug_level > 0:
                for n in node_list:
                    print 'node(' + n.label + ') cost: ' + repr(n.get_cost())
            nl_length = len(node_list)
            i = 0
            while i < nl_length:
                #if n.mapped_to == None:
                n = node_list[i]
                cluster = find_feasible_cluster(n, user_graph)
                if not cluster:
                    failed = n
                    break
                else:
                    new_node = map_node(n, user_graph, cluster)
                    #TODO append new_node into node list
                    if new_node:
                        inserted_new = False
                        for j in range((i+1),nl_length):
                            if new_node.cost > node_list[j].cost:
                                node_list.insert(j, new_node)
                                inserted_new = True
                                break
                        if not inserted_new:
                            node_list.append(new_node)
                        if debug_level > 0:
                            print 'adding new node(' + new_node.label + ') cost: ' + repr(new_node.cost)
                        nl_length += 1
                    if debug_level > 2:
                        user_graph.print_mapping()
                i += 1
            user_graph.calculate_metric()
            if failed:
                if debug_level > 0:
                    print '\nFAILURE:unable to map reservation ' + user_graph.file + ' failed on node(' + failed.label + ')\nPARTIAL MAPPING:\n'
                    user_graph.print_mapping()
                else:
                    print '\nFAILURE:unable to map reservation ' + user_graph.file + '\nMETRIC: INTER_CLUSTER_COST(' + repr(user_graph.ice_cost) + ') HOST_COST(' + repr(2*user_graph.host_cost) + ')\n'
                user_graph.mapping_failed = True
                if not show_partial_mapping:
                    user_graph.unmap()
                    removals.append(user_graph) 
            else:
                if debug_level > 0 or print_final_mapping:
                    print '\nSUCCESS:mapped reservation ' + user_graph.file + '\nFINAL MAPPING:\n'
                    user_graph.print_mapping()
                else:
                    print '\nSUCCESS:mapped reservation ' + user_graph.file + '\nMETRIC: INTER_CLUSTER_COST(' + repr(user_graph.ice_cost) + ') HOST_COST(' + repr(2*user_graph.host_cost) + ')\n'
                total_ice_cost += user_graph.ice_cost
                total_host_cost += (2*user_graph.host_cost)
    for ug in removals:
        user_graphs.remove(ug)
    print '\n\nTOTAL METRIC for all graphs: INTER_CLUSTER_COST(' + repr(total_ice_cost) + ') HOST_COST(' + repr(total_host_cost) + ')\n\n'
else:
    pc1cores[0].res_id = 'r'
    pc1cores[1].res_id = 'r'
    pc1cores[2].res_id = 'r'
    pc1cores[3].res_id = 'r'
    ixps[0].res_id = 'r'

    pc8cores[0].res_id = 'b'
    pc48cores[0].res_id = 'b'
    pc48cores[1].res_id = 'b'  
    
#draw testbed
#pos2 = networkx.graphviz_layout(testbed, prog='neato') 

#pos = networkx.graphviz_layout(testbed)
pos = {}
sw_ndx = 96
pc_ndx = 0
rtr_ndx = 48
pc_l = 1
for i in range(1,7):
    cluster = clusters[i]
    pos[cluster.sw] = (sw_ndx, 5)
    sw_ndx += 228
    for n in cluster.hw['pc1']:
        pos[n] = (pc_ndx, pc_l)
        pc_ndx += 16
        if pc_l == 1:
            pc_l = 2
        else:
            pc_l = 1
    for n in cluster.hw['ixp']:
        pos[n] = (rtr_ndx, 3)
        rtr_ndx += 70
    if 'nsp' in cluster.hw:
        for n in cluster.hw['nsp']:
            pos[n] = (rtr_ndx, 3)
            rtr_ndx += 70
    rtr_ndx += 96
    pc_ndx += 36


sw_ndx = 196
pc_ndx = 40
pc48_ndx = 196
for i in range(7,11):
    cluster = clusters[i]
    pos[cluster.sw] = (sw_ndx, 7)
    sw_ndx += 312
    
    if 'pc48' in cluster.hw:
        for n in cluster.hw['pc48']:
            pos[n] = (pc48_ndx, 9)
            pc48_ndx += 312
            #pc_ndx += 36
    for n in cluster.hw['pc8']:
        pos[n] = (pc_ndx, 10)
        pc_ndx += 36
    pc_ndx += 36

#pos2 = networkx.graphviz_layout(testbed)
#pos = {clusters[1].sw:(0,1),
#       clusters[2].sw:(1,1),
#       clusters[3].sw:(2,1),
#       clusters[4].sw:(3,1),
#       clusters[5].sw:(4,1),
#       clusters[6].sw:(5,1),
#       clusters[7].sw:(1,2),
#       clusters[8].sw:(2,2),
#       clusters[9].sw:(3,2),
#       clusters[10].sw:(4,2)}
#print pos

num_cols = 1
num_rows = 1

if user_graphs and args.ugraphs:
    num_graphs = len(user_graphs)
    num_rows = 2
    tmp = math.ceil(float(num_graphs + 1)/2)
    #tmp2 = math.ceil(tmp)
    num_cols = int(tmp)
    #print 'VISUALIZE ugraphs: rows=' + repr(num_rows) + ' cols=' + repr(num_cols) + ' tmp=' + repr(tmp) + ' tmp2=' + repr(tmp2) + ' num_graphs=' + repr(num_graphs)
    i = 2
    for ug in user_graphs:
        print '    i = ' + repr(i)
        mplot.subplot(num_cols, num_rows, i)
        ug.visualize_mapping()
        i += 1

mplot.subplot(num_cols, num_rows, 1)

ncolors = []
for n in sw10s:
    if n.res_id == 'free':
        #ncolors.append('y')
        ncolors.append('w')
    else:
        ncolors.append(n.res_id)
networkx.draw_networkx_nodes(testbed_viz, pos, nodelist=sw10s, node_color=ncolors, node_shape='s', node_size=2000) 

ncolors = []
for n in sw1s:
    if n.res_id == 'free':
        ncolors.append('w') #'b')
    else:
        ncolors.append(n.res_id)
networkx.draw_networkx_nodes(testbed_viz, pos, nodelist=sw1s, node_color=ncolors, node_shape='s', node_size=1600) 

ncolors = []
for n in pc1cores:
    if n.res_id == 'free':
        ncolors.append('w') #'r')
    else:
        ncolors.append(n.res_id)
networkx.draw_networkx_nodes(testbed, pos, nodelist=pc1cores, node_color=ncolors, node_shape='o', node_size=20) 

ncolors = []
for n in pc8cores:
    if n.res_id == 'free':
        ncolors.append('w') #'r')
    else:
        ncolors.append(n.res_id)
networkx.draw_networkx_nodes(testbed_viz, pos, nodelist=pc8cores, node_color=ncolors, node_shape='o', node_size=170) 

ncolors = []
for n in pc48cores:
    if n.res_id == 'free':
        ncolors.append('w') #'r')
    else:
        ncolors.append(n.res_id)
networkx.draw_networkx_nodes(testbed_viz, pos, nodelist=pc48cores, node_color=ncolors, node_shape='8', node_size=150) 

ncolors = []
for n in ixps:
    if n.res_id == 'free':
        ncolors.append('w') #'r')
    else:
        ncolors.append(n.res_id) #'w')
networkx.draw_networkx_nodes(testbed_viz, pos, nodelist=ixps, node_color=ncolors, node_shape='^', node_size=500) 

ncolors = []
for n in nsps:
    if n.res_id == 'free':
        ncolors.append('w') #'r')
    else:
        ncolors.append(n.res_id)
networkx.draw_networkx_nodes(testbed_viz, pos, nodelist=nsps, node_color=ncolors, node_shape='v', node_size=500)
#networkx.draw_networkx_nodes(testbed, pos, nodelist=clusters[1:7], node_color='r', node_shape='s', node_size=1000)
#networkx.draw_networkx_nodes(testbed, pos, nodelist=clusters[7:], node_color='b', node_shape='s', node_size=1400)

labels = {}
for n in testbed_viz.nodes():
    if 'sw' in n.hwtype:
        labels[n] = n.label
    else:
        labels[n] = repr(n.index)
networkx.draw_networkx_labels(testbed_viz, pos, labels) 
networkx.draw_networkx_edges(testbed_viz, pos)

mplot.axis('off')
mplot.savefig("testbed.png") # save as png
mplot.show() # display


#generate a png file  of testbed

# take in graph of testbed. nodes of different types, marked if allocated or out of commission. 
#     edges with capacity and capacity allocated.

# take in reservation graph(s), output allocation each time
# color code reservations. remove reservation by identifier
# disable specific nodes
