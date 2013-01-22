/*
 * Copyright (c) 2008-2013 Charlie Wiseman, Jyoti Parwatikar and Washington University in St. Louis.
 * All rights reserved
 *
 * Distributed under the terms of the GNU General Public License v3
 * */

#ifndef _INTERNAL_H
#define _INTERNAL_H

template <class T>
inline std::string to_string(const T& t)
{
  std::stringstream ss;
  ss << t;
  return ss.str();
}

inline int str2int(std::string s) throw()
{
  std::stringstream ss(s);
  int n;
  ss >> n;
  return n;
}

struct _link_resource;

typedef struct _node_resource
{
  // these are filled in at creation time
  std::string type;
  unsigned int label;
  std::list< boost::shared_ptr<struct _link_resource> > links;

  // this will be filled in if the node gets mapped to a physical node 
  boost::shared_ptr<struct _node_resource> mapped_node;
  bool is_mapped;

  // these are filled in later and are used for auxiliary purposes by the implementation
  bool marked; // initialized to false
  //unsigned int priority;  //initialized to 0
  unsigned int in;  //initialized to 0
  int cost; // initialized to 0
} node_resource;

typedef boost::shared_ptr<node_resource> node_resource_ptr;

typedef struct _link_resource
{
  // these are filled in at creation time
  unsigned int label;
  unsigned int capacity;
  node_resource_ptr node1;
  unsigned int node1_port;
  node_resource_ptr node2;
  unsigned int node2_port;

  // this will be filled in if the link gets mapped to a testbed path
  std::list< boost::shared_ptr<struct _link_resource> > mapped_path;
  bool is_mapped;
  
  // this is used when the link is part of the testbed graph to keep track of capacity
  //unsigned int used_capacity;

  unsigned int rload;
  unsigned int lload;
  int cost;
  unsigned int potential_rcap;//used in computation of mapping cost of a potential path
  unsigned int potential_lcap;//used in computation of mapping cost of a potential path

  // these are filled in later and are used for auxiliary purposes by the implementation
  bool marked; // initialized to false
  unsigned int in; // initialized to 0
} link_resource;

typedef boost::shared_ptr<link_resource> link_resource_ptr;

//added by JP to handle vswitches //just keeps track of vswitch and its attached leaf nodes
//typedef struct _vswitch_group
//{
// std::list<node_resource_ptr> leaf_nodes;
//boost::shared_ptr<struct _node_resource> vswitch;
//} vswitch_group;

//typedef boost::shared_ptr<_vswitch_group> vswitch_group_ptr;

//added by JP to list subnets
typedef struct _subnet_info
{
  std::list<node_resource_ptr> nodes;
  std::list<link_resource_ptr> links;
} subnet_info;

typedef boost::shared_ptr<subnet_info> subnet_info_ptr;

typedef struct _assign_info
{
  std::string type;
  int user_nodes;
  int testbed_nodes;
} assign_info;

typedef boost::shared_ptr<assign_info> assign_info_ptr;

class topology;

typedef struct _schedule_entry
{
  topology *t;
  time_t b_unix;
  time_t e_unix;
} schedule_entry;
typedef boost::shared_ptr<schedule_entry> schedule_entry_ptr;

typedef struct _link_path
{
  std::list<link_resource_ptr> path;
  node_resource_ptr sink;
  int cost;
  int sink_port;
} link_path;
typedef boost::shared_ptr<link_path> link_path_ptr;


#endif // _INTERNAL_H
