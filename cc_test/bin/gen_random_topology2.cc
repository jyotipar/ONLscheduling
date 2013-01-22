/*
 * Copyright (c) 2008-2013 Charlie Wiseman, Jyoti Parwatikar and Washington University in St. Louis.
 * All rights reserved
 *
 * Distributed under the terms of the GNU General Public License v3
 * 
*/

#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <vector>

#include <stdlib.h>
#include <sys/time.h>

#include "mappers.h"

using namespace std;

const int BIGINT = 0x7fffffff;
inline double randfrac() { return ((double) random())/BIGINT; }
inline int randint(int lo, int hi) { return lo + (random() % (hi + 1 - lo)); }

typedef struct _node
{
  std::string type;
  unsigned int num_ports;
  unsigned int used_ports;
  unsigned int label;
  bool connected;
} node;

typedef struct _port
{
  node *n;
  unsigned int port;
  bool connected;
} port; 

int main(int argc, char **argv)
{
  topology *t = new topology();

  if(argc != 3)
  {
    cout << "usage: gen_random_topology2 NRS AVG_BB_DEG" << std::endl;
    return -1;
  }

  unsigned int nrs; // normalized topology size
  double desired_node_degree; // desired average degree of user backbone nodes
  
  nrs = strtoul(argv[1],NULL,10);
  desired_node_degree = atof(argv[2]);

  struct timeval now;
  gettimeofday(&now, NULL);
  srandom((now.tv_sec << 20) + now.tv_usec);

  std::string types[4] = {"A","B","C","D"};
  unsigned int num_available[4] = {4,14,6,82};
  unsigned int num_ports[4] = {8,5,4,1};

  unsigned int backbone_size = 0;
  std::vector<node *> topology_nodes;
  for(unsigned int type=0; type<3; ++type)
  {
    for(unsigned int num=0; num<num_available[type]; ++num)
    {
      node *n = new node();
      n->type = types[type];
      n->num_ports = num_ports[type];
      n->used_ports = 0;
      n->label = 0;
      n->connected = false;
      topology_nodes.push_back(n);
    }
    backbone_size += num_available[type];
  }

  std::vector<node *> user_nodes;
  std::vector<node *>::iterator un;
  std::vector<node *>::iterator tn;

  try
  {
    int next_label = 1;
    int num_edges = 0;

    // get a node randomly to be first backbone node
    node *n = topology_nodes.at(randint(0,topology_nodes.size()-1));
    n->label = next_label;
    user_nodes.push_back(n);
    t->add_node(n->type, next_label);
    ++next_label;
    
    // randomly add additional backbone nodes, up to the desired size
    while(user_nodes.size() < nrs)
    {
      node *nn = topology_nodes.at(randint(0,topology_nodes.size()-1));
      if(nn->label != 0) continue;

      nn->label = next_label;
      user_nodes.push_back(nn);
      t->add_node(nn->type, next_label);
      ++next_label;
    }

    // form connected tree from user backbone nodes
    n = user_nodes.at(randint(0,user_nodes.size()-1));
    n->connected = true;
    
    std::vector<port *> backbone_ports;
    for(unsigned int i=0; i<n->num_ports; ++i)
    {
      port *p = new port();
      p->n = n;
      p->port = i;
      p->connected = false;
      backbone_ports.push_back(p);
    }

    for(un = user_nodes.begin(); un != user_nodes.end(); ++un)
    {
      if((*un)->connected) continue;
      (*un)->connected = true;

      int this_port_num = randint(0,(*un)->num_ports-1);
      port *p = backbone_ports.at(randint(0,backbone_ports.size()-1));
      while(p->connected) { p = backbone_ports.at(randint(0,backbone_ports.size()-1)); }

      for(unsigned int i=0; i<(*un)->num_ports; ++i)
      {
        port *np = new port();
        np->n = *un;
        np->port = i;
        if((int)i == this_port_num) { np->connected = true; }
        else { np->connected = false; }
        backbone_ports.push_back(np);
      }
      p->connected = true;

      (*un)->used_ports++;
      p->n->used_ports++;
      t->add_link(next_label, 1, (*un)->label, this_port_num, p->n->label, p->port);
      ++next_label;
      ++num_edges;
    }

    // add additional random edges until average node degree >= desired node degree
    if(user_nodes.size() > 1)
    {
      double current_avg_node_degree = ((double)num_edges)/((double)user_nodes.size());
      while(current_avg_node_degree < desired_node_degree)
      {
        port *p1 = backbone_ports.at(randint(0,backbone_ports.size()-1));
        while(p1->connected) { p1 = backbone_ports.at(randint(0,backbone_ports.size()-1)); }

        port *p2 = backbone_ports.at(randint(0,backbone_ports.size()-1));
        while(p2->connected || (p2->n->label == p1->n->label)) { p2 = backbone_ports.at(randint(0,backbone_ports.size()-1)); }

        p1->connected = true;
        p2->connected = true;
      
        p1->n->used_ports++;
        p2->n->used_ports++;
        t->add_link(next_label, 1, p1->n->label, p1->port, p2->n->label, p2->port);
        ++next_label;
        ++num_edges;

        current_avg_node_degree = ((double)num_edges)/((double)user_nodes.size());
   
        std::vector<node *>::iterator nit;
        bool seen1 = false;
        bool seen2 = false;
        for(nit = user_nodes.begin(); nit != user_nodes.end(); ++nit)
        {
          if((*nit)->used_ports == (*nit)->num_ports) continue;
          if(seen1) seen2 = true;
          else seen1 = true;
        }
        if(!seen2) break;
      }
    }

    // now fill out all unused ports with hosts
    unsigned int hosts_used = 0;
    std::vector<port *>::iterator p;
    for(p = backbone_ports.begin(); p != backbone_ports.end(); ++p)
    {
      if(hosts_used >= num_available[3]) break;
      if((*p)->connected) continue;
      (*p)->connected = true;
      (*p)->n->used_ports++;
      hosts_used++;
      
      t->add_node(types[3],next_label);
      ++next_label;
      t->add_link(next_label, 1, (*p)->n->label, (*p)->port, next_label-1, 0);
      ++next_label;
    }
  }
  catch(reservation_exception& re)
  {
    cout << "error: " << re.what() << std::endl;
  }

  t->write_to_stdout();

  if(t) delete t;
}
