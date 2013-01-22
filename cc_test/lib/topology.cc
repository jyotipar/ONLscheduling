/*
 * Copyright (c) 2008-2013 Charlie Wiseman, Jyoti Parwatikar and Washington University in St. Louis.
 * All rights reserved
 *
 * Distributed under the terms of the GNU General Public License v3
 * */

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <list>

#include <boost/shared_ptr.hpp>

#include "internal.h"
#include "exceptions.h"
#include "topology.h"

using namespace std;

topology::topology() throw()
{
  file_name = "";
}

topology::topology(std::string file) throw(reservation_exception)
{
  fstream fs(file.c_str(), fstream::in);

  if(fs.fail()) throw reservation_exception("file not found!");

  file_name = file;

  std::string s; 
  size_t found1 = 0;
  size_t found2 = 0;
  size_t found3 = 0;
  size_t found4 = 0;
  size_t found5 = 0;
  size_t found6 = 0;
  
  std::string type;
  unsigned int label;
  unsigned int capacity;
  unsigned int node1;
  unsigned int node1port;
  unsigned int node2;
  unsigned int node2port;

  getline(fs, s); // first line should be 'nodes', can ignore
  getline(fs, s);
  found1 = s.find(',',0);
  while(found1 != string::npos)
  {
    label = str2int(s.substr(0,found1));
    type = s.substr(found1+1,string::npos);
    add_node(type, label);
    getline(fs, s);
    found1 = s.find(',',0);
  }
  getline(fs, s); 
  while(!fs.eof())
  {
    found1 = s.find(',',0);
    label = str2int(s.substr(0,found1));
    found2 = s.find(',',found1+1);
    capacity = str2int(s.substr(found1+1,found2));
    found3 = s.find(',',found2+1);
    node1 = str2int(s.substr(found2+1,found3));
    found4 = s.find(',',found3+1);
    node1port = str2int(s.substr(found3+1,found4));
    found5 = s.find(',',found4+1);
    node2 = str2int(s.substr(found4+1,found5));
    found6 = s.find(',',found5+1);
    node2port = str2int(s.substr(found5+1,found6));
    add_link(label, capacity, node1, node1port, node2, node2port);
    getline(fs, s); 
  }
  
  fs.close();
  //find_vswitch_leaves();
}

topology::~topology() throw()
{
  list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    (*nit)->links.clear();
  }

  list<link_resource_ptr>::iterator lit;
  for(lit = links.begin(); lit != links.end(); ++lit)
  {
    (*lit)->mapped_path.clear();
  }

  nodes.clear();
  links.clear();
}

void topology::print_resources() throw()
{
  std::list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    cout << "hardware " << to_string((*nit)->label) << " is of type " << (*nit)->type;
    if((*nit)->is_mapped)
    {
      cout << " and is mapped to " << to_string((*nit)->mapped_node->label);
    }
    cout << endl;
  }

  std::list<link_resource_ptr>::iterator lit;
  for(lit = links.begin(); lit != links.end(); ++lit)
  {
    cout << "link " << to_string((*lit)->label) << ": (" << to_string((*lit)->node1->label) << "p" << to_string((*lit)->node1_port) << ", " << to_string((*lit)->node2->label) << "p" << to_string((*lit)->node2_port) << ") capacity " << to_string((*lit)->capacity) << " load(" << (*lit)->rload << "," << (*lit)->lload << ")";
    if((*lit)->is_mapped)
    {
      cout << " and is mapped to ";
      list<link_resource_ptr>::iterator m;
      for(m = (*lit)->mapped_path.begin(); m != (*lit)->mapped_path.end(); ++m)
      {
        cout << to_string((*m)->label) << ":" << "(" << ((*m)->node1->label) << "," << ((*m)->node2->label) << ",rl:" << ((*m)->rload) << ",ll:" << ((*m)->lload) << "),";
      }
    }
    cout << endl;
  }
}

void topology::write_to_file(std::string file) throw()
{
  fstream fs(file.c_str(), fstream::out);

  fs << "nodes" << std::endl;
  std::list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    fs << to_string((*nit)->label) << "," << (*nit)->type << std::endl;
  }

  fs << "links" << std::endl;
  std::list<link_resource_ptr>::iterator lit;
  for(lit = links.begin(); lit != links.end(); ++lit)
  {
    fs << to_string((*lit)->label) << "," << to_string((*lit)->capacity) << "," << to_string((*lit)->node1->label) << "," << to_string((*lit)->node1_port) << "," << to_string((*lit)->node2->label) << "," << to_string((*lit)->node2_port) << std::endl;
  }

  fs.close();
}

void topology::write_to_stdout() throw()
{
  cout << "nodes" << std::endl;
  std::list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    cout << to_string((*nit)->label) << "," << (*nit)->type << std::endl;
  }

  cout << "links" << std::endl;
  std::list<link_resource_ptr>::iterator lit;
  for(lit = links.begin(); lit != links.end(); ++lit)
  {
    cout << to_string((*lit)->label) << "," << to_string((*lit)->capacity) << "," << to_string((*lit)->node1->label) << "," << to_string((*lit)->node1_port) << "," << to_string((*lit)->node2->label) << "," << to_string((*lit)->node2_port) << std::endl;
  }
}

void topology::add_node(std::string type, unsigned int label) throw(reservation_exception)
{
  node_resource_ptr nrp(new node_resource());
  nrp->type = type;
  nrp->label = label;
  nrp->is_mapped = false;
  nrp->marked = false;
  //nrp->priority = 0;
  nrp->in = 0;
  nrp->cost = 0;

  list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    if((*nit)->label == nrp->label)
    {
      throw reservation_exception((std::string)"label " + to_string(label) + " already used");
    }
  }
  
  nodes.push_back(nrp);
  /*
  if (type == "V") //found vswitch //JP added
    {
      vswitch_group_ptr nvgp(new vswitch_group());
      nvgp->vswitch = nrp;
      vswitch_groups.push_back(nvgp);
    }
  */
}

void topology::add_link(unsigned int label, unsigned int capacity, unsigned int node1_label, unsigned int node1_port, unsigned int node2_label, unsigned int node2_port) throw(reservation_exception)
{
  link_resource_ptr lrp(new link_resource());
  
  bool node1_found = false;
  bool node2_found = false;

  lrp->label = label;
  lrp->capacity = capacity;
  lrp->node1_port = node1_port;
  lrp->node2_port = node2_port;
  lrp->is_mapped = false;
  lrp->rload = 0;
  lrp->lload = 0;
  lrp->marked = false;
  
  list<link_resource_ptr>::iterator lit;
  for(lit = links.begin(); lit != links.end(); ++lit)
  {
    if((*lit)->label == lrp->label)
    {
      throw reservation_exception((std::string)"label " + to_string(label) + " already used");
    }
  }

  list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    if((!node1_found) && ((*nit)->label == node1_label))
    {
      lrp->node1 = *nit;
      node1_found = true;
    }
    if((!node2_found) && ((*nit)->label == node2_label))
    {
      lrp->node2 = *nit;
      node2_found = true;
    }
  }
  
  if(!node1_found)
  {
    throw reservation_exception((std::string)"node1_label " + to_string(node1_label) + " not found");
  }
  if(!node2_found)
  {
    throw reservation_exception((std::string)"node2_label " + to_string(node2_label) + " not found");
  }

  lrp->node1->links.push_back(lrp);
  lrp->node2->links.push_back(lrp);
  links.push_back(lrp);

  if(lrp->node1->type == "I")
  {
    lrp->node1->in = lrp->node1->label;
    lrp->node2->in = lrp->node1->label;
  }
  else if(lrp->node2->type == "I")
  {
    lrp->node1->in = lrp->node2->label;
    lrp->node2->in = lrp->node2->label;
  }
}

/*
void topology::find_vswitch_leaves()//JP added to support vswitches 11/7/2012
{
  list<vswitch_group_ptr>::iterator vit;
  list<link_resource_ptr>::iterator lit;
  for(vit = vswitch_groups.begin(); vit != vswitch_groups.end(); ++vit)
    {
      for(lit = (*vit)->vswitch->links.begin(); lit != (*vit)->vswitch->links.end(); ++lit)
	{

	  node_resource_ptr onode = (*lit)->node2;
	  if ((*lit)->node2 == (*vit)->vswitch) onode = (*lit)->node1;
	  if (onode->type != "V")
	    (*vit)->leaf_nodes.push_back(onode);
	}
    }
}
*/
