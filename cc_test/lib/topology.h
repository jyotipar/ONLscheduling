/*
 * Copyright (c) 2008-2013 Charlie Wiseman, Jyoti Parwatikar and Washington University in St. Louis.
 * All rights reserved
 *
 * Distributed under the terms of the GNU General Public License v3
 * */

#ifndef _TOPOLOGY_H
#define _TOPOLOGY_H

class topology
{
  friend class reservations;

  private:
    std::list<node_resource_ptr> nodes;
    std::list<link_resource_ptr> links;
    //just keeps track of vswitches and their attached leaf nodes
    // std::list<subnet_ptr> subnets;//added by JP
    //std::list<vswitch_group_ptr> vswitch_groups; //added by JP to handle vswitches 
    //void find_vswitch_leaves();

  public:
    topology() throw();
    topology(std::string file) throw(reservation_exception);
    ~topology() throw();
   
    void print_resources() throw();
    void write_to_file(std::string file) throw();
    void write_to_stdout() throw();

    // you have to add the hw resource before you add any links for it
    void add_node(std::string type, unsigned int label) throw(reservation_exception);
    void add_link(unsigned int label, unsigned int capacity, unsigned int node1_label, unsigned int node1_port, unsigned int node2_label, unsigned int node2_port) throw(reservation_exception);
    void calculate_subnets();
    std::string file_name;
};
#endif // _TOPOLOGY_H
