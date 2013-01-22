/*
 * Copyright (c) 2008-2013 Charlie Wiseman, Jyoti Parwatikar and Washington University in St. Louis.
 * All rights reserved
 *
 * Distributed under the terms of the GNU General Public License v3
 * */

#ifndef _RESERVATIONS_H
#define _RESERVATIONS_H

class reservations
{
  private:
    std::list<schedule_entry_ptr> schedule;
    topology *base;

    std::string time_unix2str(time_t unix_time) throw();//*
    time_t time_str2unix(std::string str) throw();//*

    time_t discretize_time(time_t time, unsigned int hour_divisor) throw();//*
    time_t add_time(time_t time, unsigned int seconds) throw();//*
    time_t sub_time(time_t time, unsigned int seconds) throw();//*

    void prepare_base_topology(time_t begin, time_t end) throw();//*
    int try_reservation(topology *t, time_t begin, time_t end) throw();//*
    bool find_mapping(topology *req, std::list<node_resource_ptr> cl) throw();//*
    node_resource_ptr map_node(node_resource_ptr node, topology* req, node_resource_ptr cluster) throw();//*
    void map_edges(node_resource_ptr unode, node_resource_ptr rnode) throw();//*
    node_resource_ptr find_feasible_cluster(node_resource_ptr node, std::list<node_resource_ptr> cl, topology* req) throw();//*
    node_resource_ptr find_available_node(node_resource_ptr cluster, std::string ntype) throw();//*
    node_resource_ptr find_available_node(node_resource_ptr cluster, std::string ntype, std::list<node_resource_ptr> nodes_used) throw();//*
    void initialize_base_potential_loads();//*
    void get_subnet(topology* req, node_resource_ptr vgige, subnet_info_ptr subnet) throw();//*
    int compute_mapping_cost(node_resource_ptr cluster, node_resource_ptr node, topology* req, std::list<link_resource_ptr> mapped_edges) throw();//*
    //int find_cheapest_path(node_resource_ptr source, node_resource_ptr sink, link_resource_ptr ulink, std::list<link_resource_ptr> potential_path) throw();//*
    //int find_cheapest_path(node_resource_ptr source, int src_port, node_resource_ptr sink, int sink_port, link_resource_ptr ulink, std::list<link_resource_ptr> potential_path, std::list<node_resource_ptr> nodes_seen) throw();//*
    int find_cheapest_path_breadth(link_resource_ptr ulink, link_resource_ptr potential_path) throw();//*
    void calculate_node_costs(topology* req) throw();//*
    void calculate_edge_loads(topology* req) throw();//*
    void add_edge_load(node_resource_ptr node, int port, int load, std::list<link_resource_ptr> links_seen) throw();//*
    void unmap_reservation(topology* req) throw();
    node_resource_ptr get_new_vswitch(topology* req) throw();
    bool is_cluster_mapped(node_resource_ptr cluster) throw();

  public:

    reservations(std::string file) throw(reservation_exception);//*
    ~reservations() throw();//*

    // begin1,begin2 are strings in YYYYMMDDHHMMSS form, len is in minutes
    int make_reservation(std::string begin1, std::string begin2, unsigned int len, topology *t) throw(reservation_exception);//*
    int compute_host_cost(topology* topo);
    int compute_intercluster_cost(topology* topo);
};

#endif // _RESERVATIONS_H
