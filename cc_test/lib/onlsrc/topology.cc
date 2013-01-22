#include <string>
#include <iostream>
#include <sstream>
#include <list>

#include <boost/shared_ptr.hpp>

#include "internal.h"
#include "onldb_resp.h"
#include "topology.h"

using namespace std;
using namespace onl;

topology::topology() throw()
{
}

topology::~topology() throw()
{
  while(!nodes.empty())
  {
    node_resource_ptr hw = nodes.front();
    nodes.pop_front();

    hw->links.clear();
    hw->node_children.clear();
    hw->parent.reset();
  }

  while(!links.empty())
  {
    link_resource_ptr lnk = links.front();
    links.pop_front();

    lnk->node1.reset();
    lnk->node2.reset();
  }
}

std::string topology::lowercase(std::string s) throw()
{
 for(unsigned int i=0; i<s.length(); ++i)
 {
  s[i] = ::tolower(s[i]);
 }
 return s;
}

void topology::print_resources() throw()
{
  list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    cout << "node " << to_string((*nit)->label) << " is mapped to " << (*nit)->node << endl;
  }

  list<link_resource_ptr>::iterator lit;
  for(lit = links.begin(); lit != links.end(); ++lit)
  {
    cout << "link " << to_string((*lit)->label) << " is mapped to ";
    std::list<int>::iterator c; 
    for(c = (*lit)->conns.begin(); c != (*lit)->conns.end(); ++c)
    {
      cout << to_string(*c) << ",";
    }
    cout << endl;
  }
}

onldb_resp topology::add_node(std::string type, unsigned int label, unsigned int parent_label) throw()
{
  node_resource_ptr hrp(new node_resource());
  hrp->type = lowercase(type);
  hrp->label = label;
  hrp->is_parent = false;

  hrp->fixed = false;
  hrp->node = "";
  hrp->acl = "unused";
  hrp->cp = "unused";

  hrp->type_type = "";
  hrp->marked = false;
  hrp->level = 0;
  hrp->priority = 0;
  hrp->in = 0;
  hrp->mip_id = 0;

  bool parent_found;
  if(parent_label == 0)
  {
    parent_found = true;
  }
  else
  {
    parent_found = false;
  }

  list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    if((*nit)->label == hrp->label)
    {
      return onldb_resp(-1,(std::string)"label " + to_string(label) + " already used");
    }
    if((!parent_found) && ((*nit)->label == parent_label))
    {
      hrp->parent = *nit;
      (*nit)->is_parent = true;
      (*nit)->node_children.push_back(hrp);
      parent_found = true;
    }
  }
  
  if(!parent_found)
  {
    return onldb_resp(-1,(std::string)"parent_label " + to_string(parent_label) + " not found");
  }

  nodes.push_back(hrp);

  return onldb_resp(1,(std::string)"success");
}

onldb_resp topology::add_link(unsigned int label, unsigned int capacity, unsigned int node1_label, unsigned int node1_port, unsigned int node2_label, unsigned int node2_port) throw()
{
  link_resource_ptr lrp(new link_resource());
  
  bool node1_found = false;
  bool node2_found = false;

  lrp->label = label;
  lrp->capacity = capacity;
  lrp->node1_port = node1_port;
  lrp->node2_port = node2_port;

  lrp->marked = false;
  lrp->level = 0;
  lrp->in = 0;
  lrp->linkid = 0;
  
  list<link_resource_ptr>::iterator lit;
  for(lit = links.begin(); lit != links.end(); ++lit)
  {
    if((*lit)->label == lrp->label)
    {
      return onldb_resp(-1,(std::string)"label " + to_string(label) + " already used");
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
    return onldb_resp(-1,(std::string)"node1_label " + to_string(node1_label) + " not found");
  }
  if(!node2_found)
  {
    return onldb_resp(-1,(std::string)"node2_label " + to_string(node2_label) + " not found");
  }

  lrp->node1->links.push_back(lrp);
  //if(node1_label != node2_label) lrp->node2->links.push_back(lrp);
  //cgw, this may need to change back
  lrp->node2->links.push_back(lrp);
  links.push_back(lrp);

  return onldb_resp(1,(std::string)"success");
}

onldb_resp topology::remove_node(unsigned int label) throw()
{
  list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    if((*nit)->label == label)
    {
      nodes.erase(nit);
      return onldb_resp(1,(std::string)"success");
    }
  }
  return onldb_resp(-1,(std::string)"label " + to_string(label) + " not found");
}

onldb_resp topology::remove_link(unsigned int label) throw()
{
  list<link_resource_ptr>::iterator lit;
  for(lit = links.begin(); lit != links.end(); ++lit)
  {
    if((*lit)->label == label)
    {
      links.erase(lit);
      return onldb_resp(1,(std::string)"success");
    }
  }
  return onldb_resp(-1,(std::string)"label " + to_string(label) + " not found");
}

std::string topology::get_component(unsigned int label) throw()
{
  list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    if((*nit)->label == label)
    {
      return (*nit)->node;
    }
  }
  return "";
}

std::string topology::get_type(unsigned int label) throw()
{
  list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    if((*nit)->label == label)
    {
      return (*nit)->type;
    }
  }
  return "";
}

unsigned int topology::get_label(std::string node) throw()
{
  list<node_resource_ptr>::iterator nit;
  for(nit = nodes.begin(); nit != nodes.end(); ++nit)
  {
    if((*nit)->node == node)
    {
      return (*nit)->label;
    }
  }
  return 0;
}

void topology::get_conns(unsigned int label, std::list<int>& conn_list) throw()
{
  list<link_resource_ptr>::iterator lit;
  for(lit = links.begin(); lit != links.end(); ++lit)
  {
    if((*lit)->label == label)
    {
      conn_list = (*lit)->conns;
      return;
    }
  }
}
