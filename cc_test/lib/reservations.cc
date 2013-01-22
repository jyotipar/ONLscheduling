/*
 * Copyright (c) 2008-2013 Charlie Wiseman, Jyoti Parwatikar and Washington University in St. Louis.
 * All rights reserved
 *
 * Distributed under the terms of the GNU General Public License v3
 * */

#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <list>
#include "time.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <boost/shared_ptr.hpp>

//#include <glpk.h>

#include "internal.h"
#include "exceptions.h"
#include "topology.h"
#include "reservations.h"

#define MAX_INTERCLUSTER_CAPACITY 10
#define UNUSED_CLUSTER_COST 50
#define CANT_SPLIT_VGIGE_COST 50
#define USER_UNUSED_CLUSTER_COST 20

using namespace std;

typedef struct _i_score
{
  int start_node;
  int score;
} i_score;
typedef boost::shared_ptr<i_score> i_score_ptr;

/*
bool user_sort_comp(assign_info_ptr i, assign_info_ptr j)
{
  return(i->user_nodes.size() < j->user_nodes.size());
}


bool base_sort_comp(node_resource_ptr i, node_resource_ptr j)
{
  return(i->priority < j->priority);
}


bool score_sort_comp(i_score_ptr i, i_score_ptr j)
{
  if(i->score == 0) return true;
  if(j->score == 0) return false;
  return(i->score > j->score);
}
*/
bool in_list(node_resource_ptr node, std::list<node_resource_ptr> nodes)
{
  std::list<node_resource_ptr>::iterator nit;
  for (nit = nodes.begin(); nit != nodes.end(); ++nit)
    {
      if ((*nit) == node) return true;
    }
  return false;
}

bool in_list(link_resource_ptr link, std::list<link_resource_ptr> links)
{
  std::list<link_resource_ptr>::iterator lit;
  for (lit = links.begin(); lit != links.end(); ++lit)
    {
      if ((*lit) == link) return true;
    }
  return false;
}


int calculate_edge_cost(int rload, int lload)
{
  int rtraffic = rload;
  int ltraffic = lload;
  int rtn = -1;
  if (rtraffic > MAX_INTERCLUSTER_CAPACITY)
    rtraffic = MAX_INTERCLUSTER_CAPACITY;
  if (ltraffic > MAX_INTERCLUSTER_CAPACITY)
    ltraffic = MAX_INTERCLUSTER_CAPACITY;
  rtn = rtraffic + ltraffic + (fabs(rtraffic - ltraffic)/2);
  return rtn;
}

reservations::reservations(std::string file) throw(reservation_exception)
{
  base = new topology(file);
  std::list<node_resource_ptr>::iterator n;
  std::list<link_resource_ptr>::iterator l;
  node_resource_ptr other_node;
  
  //cout << "TESTBED:INTERCLUSTER LINKS" << endl;
  for(n=base->nodes.begin(); n!=base->nodes.end(); ++n)
  {
    if ((*n)->type == "I") 
      {
	(*n)->in = (*n)->label;
	for(l = (*n)->links.begin(); l != (*n)->links.end(); ++l)
	  {
	    if ((*l)->node1 == (*n)) other_node = (*l)->node2;
	    else other_node = (*l)->node1;
	    if (other_node->type != "I")
	      other_node->in = (*n)->label;
	    else
	      {
		(*l)->in = 1;
		//cout << (*l)->label << "(" << (*l)->node1->label << "," << (*l)->node2->label << ")" << endl;
	      }
	  }
      }
  }
  //cout << endl << endl;
}

reservations::~reservations() throw()
{
  std::list<schedule_entry_ptr>::iterator s;
  for(s=schedule.begin(); s!=schedule.end(); ++s)
  {
    delete (*s)->t;
  }
  schedule.clear();

  if(base) delete base;
}

std::string reservations::time_unix2str(time_t unix_time) throw()
{
  struct tm *stm = localtime(&unix_time);

  char char_str[16];
  sprintf(char_str,"%04d%02d%02d%02d%02d%02d",stm->tm_year+1900,stm->tm_mon+1,stm->tm_mday,stm->tm_hour,stm->tm_min,stm->tm_sec);
  std::string str = char_str;
  return str;
}

time_t reservations::time_str2unix(std::string str_time) throw()
{
  struct tm stm;

  if(str_time.length() == 14)
  {
    // YYYYMMDDhhmmss
    stm.tm_sec  = str2int(str_time.substr(12,2));
    stm.tm_min  = str2int(str_time.substr(10,2));
    stm.tm_hour = str2int(str_time.substr(8,2));
    stm.tm_mday = str2int(str_time.substr(6,2));
    stm.tm_mon  = str2int(str_time.substr(4,2)) - 1;
    stm.tm_year = str2int(str_time.substr(0,4)) - 1900;
  }
  else
  {
    // YYYY-MM-DD hh:mm:ss
    stm.tm_sec  = str2int(str_time.substr(17,2));
    stm.tm_min  = str2int(str_time.substr(14,2));
    stm.tm_hour = str2int(str_time.substr(11,2));
    stm.tm_mday = str2int(str_time.substr(8,2));
    stm.tm_mon  = str2int(str_time.substr(5,2)) - 1;
    stm.tm_year = str2int(str_time.substr(0,4)) - 1900;
  }

  stm.tm_isdst = -1;
  return mktime(&stm);
}

time_t reservations::discretize_time(time_t unix_time, unsigned int hour_divisor) throw()
{
  struct tm *stm = localtime(&unix_time);
  unsigned int time_chunk = 60/hour_divisor;
  stm->tm_sec = 0;
  stm->tm_min = (stm->tm_min / time_chunk) * time_chunk;
  return mktime(stm);
}

time_t reservations::add_time(time_t unix_time, unsigned int seconds) throw()
{
  struct tm *stm = localtime(&unix_time);
  stm->tm_sec += seconds;
  return mktime(stm);
} 

time_t reservations::sub_time(time_t unix_time, unsigned int seconds) throw()
{
  struct tm *stm = localtime(&unix_time);
  stm->tm_sec -= seconds;
  return mktime(stm);
} 

void reservations::prepare_base_topology(time_t begin, time_t end) throw()
{
  std::list<node_resource_ptr>::iterator n;
  std::list<link_resource_ptr>::iterator l;
  std::list<link_resource_ptr>::iterator ll;
 
  for(n=base->nodes.begin(); n!=base->nodes.end(); ++n)
  {
    (*n)->is_mapped = false;
  }
  for(l=base->links.begin(); l!=base->links.end(); ++l)
  {
    (*l)->is_mapped = false;
    (*l)->rload = 0;
    (*l)->lload = 0;
  }

  std::list<schedule_entry_ptr>::iterator s;
  for(s=schedule.begin(); s!=schedule.end(); ++s)
  {
    if(((*s)->b_unix < end) && ((*s)->e_unix > begin))
    {
      for(n=(*s)->t->nodes.begin(); n!=(*s)->t->nodes.end(); ++n)
      {
        (*n)->mapped_node->is_mapped = true;
      }
      for(l=(*s)->t->links.begin(); l!=(*s)->t->links.end(); ++l)
      {
	node_resource_ptr last_seen = (*l)->node1->mapped_node;
        for(ll=(*l)->mapped_path.begin(); ll!=(*l)->mapped_path.end(); ++ll)
        {
          (*ll)->is_mapped = true;
	  //compute loads in either direction
	  if (last_seen == (*ll)->node1)
	    {
	      (*ll)->rload += (*l)->rload;
	      (*ll)->lload += (*l)->lload;
	      last_seen = (*ll)->node2;
	    }
	  else
	    {
	      (*ll)->rload += (*l)->lload;
	      (*ll)->lload += (*l)->rload;
	      last_seen = (*ll)->node1;
	    }
          //(*ll)->used_capacity += (*l)->capacity;
        }
      }
    }
  }
}

int reservations::try_reservation(topology *t, time_t begin, time_t end) throw()
{
  //first create a list of the nodes separated by type
  std::list<assign_info_ptr> assign_list;
  std::list<assign_info_ptr>::iterator ait;

  std::list<node_resource_ptr>::iterator nit;
  std::list<link_resource_ptr>::iterator lit;
  std::list<node_resource_ptr> cluster_list;

  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    //(*nit)->marked = false;

    for(ait = assign_list.begin(); ait != assign_list.end(); ++ait)
    {
      if((*ait)->type == (*nit)->type)
      {
        (*ait)->user_nodes += 1;
        break;
      }
    }
    if(ait == assign_list.end())
    {
      assign_info_ptr newnode(new assign_info());
      newnode->type = (*nit)->type;
      newnode->user_nodes = 1;
      assign_list.push_back(newnode);
    }
  }

  // sort the requested list by increasing number of comps to facilitate matching
  //  assign_list.sort(user_sort_comp);

  // next prepare the base topology and add its stuff to the assign_list
  prepare_base_topology(begin, end);

  for(nit = base->nodes.begin(); nit != base->nodes.end(); ++nit)
  {
    //(*nit)->marked = false;
    if((*nit)->is_mapped == true) { continue; }

    for(ait = assign_list.begin(); ait != assign_list.end(); ++ait)
    {
      if((*ait)->type == (*nit)->type || ((*ait)->type == "V" && (*nit)->type == "I"))
      {
        (*ait)->testbed_nodes += 1;
        break;
      }
    }
  }
  
  // check that the the number of each requested type is <= to the number available
  for(ait = assign_list.begin(); ait != assign_list.end(); ++ait)
  {
    if ((*ait)->type == "V") continue; //JP added 11/7/2012
    if((*ait)->user_nodes > (*ait)->testbed_nodes)
    {
      while (!assign_list.empty())
      {
        assign_list.pop_front();
      }
      return 1;
    }
  }
  while (!assign_list.empty())
    {
      assign_list.pop_front();
    }
      
  //unsigned int num_i_nodes = 0;
  for(nit = base->nodes.begin(); nit != base->nodes.end(); ++nit)
  {
    if((*nit)->type == "I") cluster_list.push_back(*nit);
  }

  // finally, try to find a mapping 
    for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
    {
      //(*nit)->marked = false;
      (*nit)->is_mapped = false;
      (*nit)->cost = 0;
    }
    for(lit = t->links.begin(); lit != t->links.end(); ++lit)
    {
      //(*lit)->marked = false;
      (*lit)->is_mapped = false;
    }

    if(find_mapping(t, cluster_list))
    {
      schedule_entry_ptr newmap(new schedule_entry());
      newmap->t = t;
      newmap->b_unix = begin;
      newmap->e_unix = end;
      schedule.push_back(newmap);
      return 2;
    }
  return 0;
}

bool reservations::find_mapping(topology *req, std::list<node_resource_ptr> cl) throw()
{
  std::list<node_resource_ptr>::iterator reqnit;
  std::list<node_resource_ptr>::iterator it;


  bool inserted_new = false;
  std::list<node_resource_ptr> ordered_nodes;


  calculate_node_costs(req);

  for(reqnit = req->nodes.begin(); reqnit != req->nodes.end(); ++reqnit)
    {
      inserted_new = false;
      for(it = ordered_nodes.begin(); it != ordered_nodes.end(); ++it)
	{
	  if ((*reqnit)->cost > (*it)->cost)
	    {
	      ordered_nodes.insert(it, *reqnit);
	      inserted_new = true;
	      break;
	    }
	}
      if (!inserted_new)
	ordered_nodes.push_back(*reqnit);
    }

  node_resource_ptr fcluster;
  node_resource_ptr new_node;
  for(reqnit = ordered_nodes.begin(); reqnit != ordered_nodes.end(); ++reqnit)
    {
      fcluster = find_feasible_cluster(*reqnit, cl, req);
      if (fcluster)
	{
	  //new_node = NULL;
	  new_node = map_node(*reqnit, req, fcluster);
	  //if new_node is returned then add it into the ordered node list
	  if (new_node)
	    {
	      inserted_new = false;
	      bool at_start = false;
	      for (it = ordered_nodes.begin(); it != ordered_nodes.end(); ++it)
		{
		  if (at_start && (*it)->cost < new_node->cost)
		    {
		      ordered_nodes.insert(it, new_node);
		      inserted_new = true;
		      break;
		    }
		  if ((*it) == (*reqnit)) at_start = true; //first get to the point we were at when we created the new node
		}
	      if (!inserted_new) ordered_nodes.push_back(new_node);
	    } 
	}
      else
	{
	  cout << " reservation failed on node " << (*reqnit)->label << endl;
          //report_metrics(req, al);
          unmap_reservation(req);//al);
	  return false;
	}
    }
  return true;
}

node_resource_ptr
reservations::map_node(node_resource_ptr node, topology* req, node_resource_ptr cluster) throw()
{
  std::list<node_resource_ptr>::iterator nit;
  std::list<link_resource_ptr>::iterator lit;

  node_resource_ptr rnode;
  node_resource_ptr nullnode;
  //if (node->marked) return nullnode; //it's been map_node was already called
  //node->marked = true;
  if (node->is_mapped) 
    {
      rnode = node->mapped_node;
    }
  else 
    {
      std::list<node_resource_ptr> nodes_used;
      rnode = find_available_node(cluster, node->type, nodes_used);
      node->is_mapped = true;
      node->mapped_node = rnode;
      node->in = cluster->in;
      rnode->is_mapped = true;
    }
  map_edges(node, rnode);
  //now try and map as many leaf neighbors as possible
  node_resource_ptr neighbor;
  std::list<node_resource_ptr> unmapped_nodes;
  for (lit = node->links.begin(); lit != node->links.end(); ++lit)
    {
      if ((*lit)->node1 == node) neighbor = (*lit)->node2;
      else neighbor = (*lit)->node1;
      if (neighbor->type == "V" || neighbor->is_mapped) continue; //skip it if it's already mapped or a vgige
      std::list<link_resource_ptr> nmapped_edges;
      std::list<link_resource_ptr>::iterator nlit;
      for (nlit = neighbor->links.begin(); nlit != neighbor->links.end(); ++nlit)
	{
	  if (((*nlit)->node1 == neighbor && (*nlit)->node2->is_mapped) ||
	      ((*nlit)->node2 == neighbor && (*nlit)->node1->is_mapped))
	    nmapped_edges.push_back(*nlit);
	}
      int cost = compute_mapping_cost(cluster, neighbor, req, nmapped_edges);
      if (cost >= 0)
	{
	  node_resource_ptr nrnode = find_available_node(cluster, neighbor->type);
	  neighbor->is_mapped = true;
	  neighbor->mapped_node = nrnode;
	  neighbor->in = cluster->in;
	  nrnode->is_mapped = true;
	}
      else unmapped_nodes.push_back(neighbor);
    }
  map_edges(node, rnode);//call map_edges again since may be more edges now that neighbors have been mapped
    
  //if this is a vswitch and we have more than 1 unmapped leaf split the node and insert a new node into the graph. return the new node
  //need to make sure the unmapped nodes do not contribute a load that exceeds the bandwidth of the interswitch edge
  if (node->type == "V" && unmapped_nodes.size() > 1)
    {
      //see if there is a cluster that we can map the split switch to with all of its leaves 
      std::list<node_resource_ptr> vgige_nodes;
      link_resource_ptr vgige_lnk(new link_resource());
      vgige_lnk->node1 = node;
      vgige_lnk->node2 = node;
      vgige_lnk->rload = 0;
      vgige_lnk->lload = 0;
      //bool can_split = false;

      //first calculate the loads for the new link created between the split vgige parts
      for (lit = node->links.begin(); lit != node->links.end(); ++lit)
	{
	  if ((*lit)->node1 == node)
	    {
	      if (in_list((*lit)->node2, unmapped_nodes)) 
		{
		  vgige_lnk->rload += (*lit)->lload;
		  if ((*lit)->node2->type != "V") vgige_nodes.push_back((*lit)->node2);
		}
	      else vgige_lnk->lload += (*lit)->rload;
	    }
	  else
	    {
	      if (in_list((*lit)->node1, unmapped_nodes))
		{
		  vgige_lnk->rload += (*lit)->rload;
		  if ((*lit)->node1->type != "V") vgige_nodes.push_back((*lit)->node1);
		}
	      else vgige_lnk->lload += (*lit)->lload;
	    }
	}
      //if the new link requires less than or equal the intercluster capacity, 
      //see if we can find a cluster that will accommodate the new switch and it's neighbors
      if ((vgige_lnk->rload > MAX_INTERCLUSTER_CAPACITY) || (vgige_lnk->lload > MAX_INTERCLUSTER_CAPACITY)) 
	{
	  return nullnode;
	}
      int ncost = 0;//used to calculate the new node's cost and the new cost of the old node
      node_resource_ptr new_vgige = get_new_vswitch(req);
      vgige_lnk->node1 = node;
      vgige_lnk->node2 = new_vgige;
      vgige_lnk->node1_port = 0;
      vgige_lnk->node2_port = 0;
      for (lit = node->links.begin(); lit != node->links.end();)
	{
	  if ((*lit)->node1 == node && in_list((*lit)->node2, unmapped_nodes)) 
	    {
	      //remove link and add to new_vgige
	      new_vgige->links.push_back(*lit);
	      (*lit)->node1 = new_vgige;
	      ncost += (*lit)->cost;
	      node->links.erase(lit);
	    }
	  else if ((*lit)->node2 == node && in_list((*lit)->node1, unmapped_nodes)) 
	    {
	      //remove link and add to new_vgige
	      new_vgige->links.push_back(*lit);
	      (*lit)->node2 = new_vgige;
	      ncost += (*lit)->cost;
	      node->links.erase(lit);
	    }
	  else ++lit;
	}
      vgige_lnk->cost = calculate_edge_cost(vgige_lnk->rload, vgige_lnk->lload);
      node->links.push_back(vgige_lnk);
      node->cost = node->cost - ncost + vgige_lnk->cost;
      new_vgige->links.push_back(vgige_lnk);
      new_vgige->cost = ncost + vgige_lnk->cost;
      req->nodes.push_back(new_vgige);
      return new_vgige;
    }

  return nullnode;
}

node_resource_ptr
reservations::get_new_vswitch(topology* req) throw()
{
  unsigned int newlabel = 1;
  std::list<node_resource_ptr>::iterator nit;
  bool found = false;
  while(!found)
    {
      found = true;
      for (nit = req->nodes.begin(); nit != req->nodes.end(); ++nit)
	{
	  if ((*nit)->label == newlabel)
	    {
	      ++newlabel;
	      found = false;
	      break;
	    }
	}
    }
  node_resource_ptr newnode(new node_resource());
  newnode->label = newlabel;
  newnode->type = "V";
  newnode->is_mapped = false;
  //newnode->marked = false;
  //nrp->priority = 0;
  newnode->in = 0;
  newnode->cost = 0;
  return newnode;
}

void
reservations::map_edges(node_resource_ptr unode, node_resource_ptr rnode) throw()
{
  std::list<link_resource_ptr>::iterator lit;
  node_resource_ptr source;
  node_resource_ptr sink;
  initialize_base_potential_loads();
  for(lit = unode->links.begin(); lit != unode->links.end(); ++lit)
    {
      if ((*lit)->is_mapped) continue;
      if ((*lit)->node1 == unode)
	{
	  if (!(*lit)->node2->is_mapped) continue;
	  source = rnode;
	  sink = (*lit)->node2->mapped_node;
	}
      else
	{
	  if (!(*lit)->node1->is_mapped) continue;
	  sink = rnode;
	  source = (*lit)->node1->mapped_node;
	}

      //std::list<link_resource_ptr> found_path;
      link_resource_ptr found_path(new link_resource());
      found_path->node1 = source;
      if ((*lit)->node1->type == "V") found_path->node1_port = -1;
      else found_path->node1_port = (*lit)->node1_port;
      found_path->node2 = sink;
      if ((*lit)->node2->type == "V") found_path->node2_port = -1;
      else found_path->node2_port = (*lit)->node2_port;
      std::list<link_resource_ptr>::iterator fpit;
      int pcost = find_cheapest_path_breadth(*lit, found_path);
      if (pcost >= 0)
	{
	  cout << "mapping link " << to_string((*lit)->label) << ": (" << to_string((*lit)->node1->label) << "p" << to_string((*lit)->node1_port) << ", " << to_string((*lit)->node2->label) << "p" << to_string((*lit)->node2_port) << ") capacity " << to_string((*lit)->capacity) << " load(" << (*lit)->rload << "," << (*lit)->lload << ") mapping to ";
	  int l_rload = (*lit)->rload;
	  if (l_rload > MAX_INTERCLUSTER_CAPACITY) l_rload =  MAX_INTERCLUSTER_CAPACITY;
	  int l_lload = (*lit)->lload;
	  if (l_lload > MAX_INTERCLUSTER_CAPACITY) l_lload =  MAX_INTERCLUSTER_CAPACITY;

	  node_resource_ptr last_visited = source;
	  node_resource_ptr other_node;
	  bool port_matters = true;
	  for (fpit = found_path->mapped_path.begin(); fpit != found_path->mapped_path.end(); ++fpit)
	    {
	      (*lit)->mapped_path.push_back(*fpit);
	      //update loads on intercluster links
		  if (last_visited->type == "I") port_matters = false;
		  if (((*fpit)->node1 == last_visited) && (!port_matters || (*lit)->node1_port == (*fpit)->node1_port))
		    {
		      if ((*fpit)->in > 0 && ((*fpit)->node1 != (*fpit)->node2))//it's an intercluster link
			{
			  (*fpit)->rload += l_rload;
			  (*fpit)->lload += l_lload;
			  (*fpit)->potential_rcap -= l_rload;
			  (*fpit)->potential_lcap -= l_lload;
			}
		      last_visited = (*fpit)->node2;
		    }
		  else if (((*fpit)->node2 == last_visited) && (!port_matters || (*lit)->node1_port == (*fpit)->node2_port))
		    {
		      if ((*fpit)->in > 0 && ((*fpit)->node1 != (*fpit)->node2))//it's an intercluster link
			{
			  (*fpit)->rload += l_lload;
			  (*fpit)->lload += l_rload;
			  (*fpit)->potential_rcap -= l_lload;
			  (*fpit)->potential_lcap -= l_rload;
			}
		      last_visited = (*fpit)->node1;
		    }
	      cout << ((*fpit)->label) << ":" << "(" << ((*fpit)->node1->label) << "," << ((*fpit)->node2->label) << ",rl:" << ((*fpit)->rload) << ",ll:" << ((*fpit)->lload) << "),";
	    }
	  (*lit)->is_mapped = true;
	  cout << endl;
	}
    }
}

node_resource_ptr
reservations::find_feasible_cluster(node_resource_ptr node, std::list<node_resource_ptr> cl, topology* req) throw()
{
  std::list<link_resource_ptr> mapped_edges;
  std::list<node_resource_ptr>::iterator clnit; //clusters
  std::list<link_resource_ptr>::iterator lit;

  if (node->is_mapped) //if the node is already mapped just return the cluster index of the mapped node
    {
      for(clnit = cl.begin(); clnit != cl.end(); ++clnit)
	{
	  if (node->in == (*clnit)->in)
	    return (*clnit);
	}
    }
  //make a list of links where the end point has already been mapped
  for(lit = node->links.begin(); lit != node->links.end(); ++lit)
    {
      if (((*lit)->node1 == node && (*lit)->node2->is_mapped) ||
	  ((*lit)->node2 == node && (*lit)->node1->is_mapped))
	mapped_edges.push_back(*lit);
    }

  node_resource_ptr rtn_cluster;
  int cluster_cost = -1;
  int current_cost = -1;

  //look through the clusters to see if we can find one that works
  cout << "cluster cost for node:" << node->label << " -- ";
  for (clnit = cl.begin(); clnit != cl.end(); ++clnit)
    {
      cluster_cost = compute_mapping_cost(*clnit, node, req, mapped_edges);
      cout << "(c" << (*clnit)->label << ", " << cluster_cost << ")";
      if (cluster_cost >= 0 && (cluster_cost < current_cost || !rtn_cluster))
	{
	  rtn_cluster = *clnit;
	  current_cost = cluster_cost;
	}
    }
  cout << endl;
  //if (rtn_cluster != NULL)
  return rtn_cluster;
  //else return NULL;
}

node_resource_ptr
reservations::find_available_node(node_resource_ptr cluster, std::string ntype) throw()
{
  std::list<node_resource_ptr> nodes_used;
  return (find_available_node(cluster, ntype, nodes_used));
}

node_resource_ptr
reservations::find_available_node(node_resource_ptr cluster, std::string ntype, std::list<node_resource_ptr> nodes_used) throw()
{
  node_resource_ptr rtn;
  std::list<link_resource_ptr>::iterator clusterlit;
  std::list<node_resource_ptr>::iterator nit;
  if (ntype == "V" && !in_list(cluster, nodes_used)) return cluster;
  for (clusterlit = cluster->links.begin(); clusterlit != cluster->links.end(); ++clusterlit)
    {
      node_resource_ptr n;
      if ((*clusterlit)->node1 == cluster && !((*clusterlit)->node2->is_mapped) && ((*clusterlit)->node2->type == ntype))
	{
	  n = (*clusterlit)->node2;
	}
      if ((*clusterlit)->node2 == cluster && !((*clusterlit)->node1->is_mapped) && ((*clusterlit)->node1->type == ntype))
	{
	  n = (*clusterlit)->node1;
	}
      if (n)//check if node found was already used in this computation for a different mapping
	{
	  bool node_in_use = false;
	  for (nit = nodes_used.begin(); nit != nodes_used.end(); ++nit)
	    {
	      if ((*nit) == n)
		{
		  node_in_use = true;
		  break;
		}
	    }
	  if (!node_in_use)
	    {
	      rtn = n;
	      break;
	    }
	}
    }
  return rtn;
}

void
reservations::initialize_base_potential_loads()
{
  std::list<link_resource_ptr>::iterator lit;
  for (lit = base->links.begin(); lit != base->links.end(); ++lit)
    {
      if ((*lit)->in > 0) //we only care about intercluster links
	{
	  (*lit)->potential_lcap = ((*lit)->capacity) - ((*lit)->lload);
	  (*lit)->potential_rcap = ((*lit)->capacity) - ((*lit)->rload);
	}
    }
}

void
reservations::get_subnet(topology* req, node_resource_ptr vgige, subnet_info_ptr subnet) throw()
{
  std::list<link_resource_ptr>::iterator lit;
  std::list<link_resource_ptr>::iterator snlit;
  std::list<node_resource_ptr>::iterator pnit;
  node_resource_ptr n;

  std::list<node_resource_ptr> pending_nodes;
  pending_nodes.push_back(vgige);
  subnet->nodes.push_back(vgige);

  while(!pending_nodes.empty())
    {
      pnit = pending_nodes.begin();
      for(lit = (*pnit)->links.begin(); lit != (*pnit)->links.end(); ++lit)
	{
	  node_resource_ptr n;
	  if ((*lit)->node1 == (*pnit)) n = (*lit)->node2;
	  else n = (*lit)->node1;
	  //check if we've already seen this node
	  if (!in_list(n, subnet->nodes)) 
	    {
	      subnet->nodes.push_back(n);
	      if (!in_list((*lit), subnet->links)) 
		{
		  subnet->links.push_back(*lit);
		}
	      if ((n->type == "V") && (!in_list(n, pending_nodes))) pending_nodes.push_back(n);
	    }
	}
      pending_nodes.erase(pnit);
    }
}


bool
reservations::is_cluster_mapped(node_resource_ptr cluster) throw()
{
  std::list<link_resource_ptr>::iterator lit;
  for (lit = cluster->links.begin(); lit != cluster->links.end(); ++lit)
    {
      if (((*lit)->node1 == cluster && (*lit)->node2->is_mapped && ((*lit)->node2->type != "I")) ||
	  ((*lit)->node2 == cluster && (*lit)->node1->is_mapped && ((*lit)->node1->type != "I")))
	return true;
    }
  return false;
}

int
reservations::compute_mapping_cost(node_resource_ptr cluster, node_resource_ptr node, topology* req, std::list<link_resource_ptr> mapped_edges) throw()
{
  int cluster_cost = 0;
  int cin = cluster->in;
  std::list<link_resource_ptr>::iterator clusterlit;
  std::list<node_resource_ptr>::iterator reqnit;
  std::list<node_resource_ptr>::iterator clusterit;
  std::list<link_resource_ptr>::iterator lit;
  std::list<node_resource_ptr> nodes_used;
  node_resource_ptr n;
  //first find an available node in the cluster to map to //probably need to deal with hwclusters here
  if (node->type == "V")
    n = cluster;
  else
    n = find_available_node(cluster, node->type, nodes_used);
  if (!n) return -1; //we couldn't find an available node of the right type in this cluster
  
  if (!is_cluster_mapped(cluster)) cluster_cost += UNUSED_CLUSTER_COST; //penalize for not being mapped to any user_graph
  else //penalize if haven't been used for this user_graph yet
    {
      bool in_use = false;
      for(reqnit = req->nodes.begin(); reqnit != req->nodes.end(); ++reqnit)
	{
	  if ((*reqnit)->is_mapped && (*reqnit)->mapped_node->in == cin)
	    {
	      in_use = true;
	      break;
	    }
	}
      if (!in_use) cluster_cost += USER_UNUSED_CLUSTER_COST;
    }

  //if the node is a vgige, we need to check if either an edge or vgige in it's subnet is already allocated to this cluster
  if (node->type == "V")
    {
      subnet_info_ptr subnet(new subnet_info());
      get_subnet(req, node, subnet);
      std::list<node_resource_ptr>::iterator snnit;
      std::list<link_resource_ptr>::iterator snlit;
      for (snnit = subnet->nodes.begin(); snnit != subnet->nodes.end(); ++snnit)
	{
	  if ((*snnit) == node) continue;
	  if ((*snnit)->is_mapped && (*snnit)->type == "V" && (*snnit)->in == cin)
	    {
	      cout << "compute_mapping_cost cluster " << cin << " no mapping. vgige " << (*snnit)->label << " already mapped here" << endl;
	      return -1;
	    }
	}
      for (snlit = subnet->links.begin(); snlit != subnet->links.end(); ++snlit)
	{
	  if ((*snlit)->is_mapped)
	    {
	      for(lit = (*snlit)->mapped_path.begin(); lit != (*snlit)->mapped_path.end(); ++lit)
		{
		  if ((*lit)->node1->in == cin || (*lit)->node2->in == cin)
		    {
		      cout << "compute_mapping_cost cluster " << cin << " no mapping. link " << (*snlit)->label << " already mapped here" << endl;
		      return -1;
		    }
		}
	    }
	}
    }

  //look through all the edges with a mapped node and make sure there is a feasible path
  //first set testbed links' potential loads equal to their actual loads
  initialize_base_potential_loads();

  node_resource_ptr source;
  node_resource_ptr sink;
  for (lit = mapped_edges.begin(); lit != mapped_edges.end(); ++lit)
    {
      if ((*lit)->is_mapped) continue;
      if ((*lit)->node1 == node)
	{
	  source = n;
	  sink = (*lit)->node2->mapped_node;
	}
      else
	{
	  source = (*lit)->node1->mapped_node;
	  sink = n;
	}
      // std::list<link_resource_ptr> potential_path;
      link_resource_ptr potential_path(new link_resource());
      potential_path->node1 = source;
      if ((*lit)->node1->type == "V") potential_path->node1_port = -1;
      else potential_path->node1_port = (*lit)->node1_port;
      potential_path->node2 = sink;
      if ((*lit)->node2->type == "V") potential_path->node2_port = -1;
      else potential_path->node2_port = (*lit)->node2_port;
      int pe_cost = find_cheapest_path_breadth(*lit, potential_path);
      if (pe_cost < 0)
	{
	  cout << "compute_mapping_cost node:" << node->label << " failed to find a path for link " << (*lit)->label << " mapped nodes:(" << source->label << "," << sink->label << ")" << endl;
	  return -1;
	}
      else //valid path was found
	{
	  cluster_cost += pe_cost;
	  std::list<link_resource_ptr>::iterator plit;
	  int l_rload = (*lit)->rload;
	  if (l_rload > MAX_INTERCLUSTER_CAPACITY) l_rload =  MAX_INTERCLUSTER_CAPACITY;
	  int l_lload = (*lit)->lload;
	  if (l_lload > MAX_INTERCLUSTER_CAPACITY) l_lload =  MAX_INTERCLUSTER_CAPACITY;
	  
	  //update potential loads on intercluster links
	  node_resource_ptr last_visited = source;
	  node_resource_ptr other_node;
	  bool port_matters = true;
	  for (plit = potential_path->mapped_path.begin(); plit != potential_path->mapped_path.end(); ++plit)
	    {
	      if ((*plit)->in > 0 && ((*plit)->node1 != (*plit)->node2))
		{
		  if (last_visited->type == "I") port_matters = false;
		  if (((*plit)->node1 == last_visited) && (!port_matters || (*lit)->node1_port == (*plit)->node1_port))
		    {
		      (*plit)->potential_rcap -= l_rload;
		      (*plit)->potential_lcap -= l_lload;
		      last_visited = (*plit)->node2;
		    }
		  else if (((*plit)->node2 == last_visited) && (!port_matters || (*lit)->node1_port == (*plit)->node2_port))
		    {
		      (*plit)->potential_rcap -= l_lload;
		      (*plit)->potential_lcap -= l_rload;
		      last_visited = (*plit)->node1;
		    }
		}
	    }
	}
    }
  //look at the potential cost of the neighbor nodes we can't map to this cluster  
  //order unmapped leaf neighbors by cost try and map highest cost first
  node_resource_ptr neighbor;
  std::list<node_resource_ptr> lneighbors;
  std::list<int> neighbor_cost;
  std::list<node_resource_ptr>::iterator lnit;
  std::list<int>::iterator ncit;
  int i = 0;
  //create a list of ordered unmapped leaf neighbors
  for (lit = node->links.begin(); lit != node->links.end(); ++lit)
    {
      bool added = false;
      ncit = neighbor_cost.begin();
      if ((*lit)->node1 == node) neighbor = (*lit)->node2;
      else neighbor = (*lit)->node1;
      //if this is not a leaf node ignore or already mapped
      if (neighbor->type == "V" || neighbor->is_mapped || node == neighbor) continue;
      added = false;
      for (lnit = lneighbors.begin(); lnit != lneighbors.end(); ++lnit)
	{
	  if (neighbor->cost >= (*lnit)->cost)
	    {
	      lneighbors.insert(lnit, neighbor);
	      added = true;
	      neighbor_cost.insert(ncit, (*lit)->cost);
	      break;
	    }
	  ++ncit;
	}
      if (!added && (neighbor != node)) 
	{
	  lneighbors.push_back(neighbor);
	  neighbor_cost.push_back((*lit)->cost);
	}
    }
  
  //go through ordered list starting with highest cost, unmapped, leaf neighbor
  std::list<node_resource_ptr> unmapped_nodes;
  node_resource_ptr available_node;
  nodes_used.clear();
  nodes_used.push_back(n);
  ncit = neighbor_cost.begin();
  for (lnit = lneighbors.begin(); lnit != lneighbors.end(); ++lnit)
    {
      available_node = find_available_node(cluster, (*lnit)->type, nodes_used);
      if (!available_node)
	{
	  unmapped_nodes.push_back(*lnit);
	  cluster_cost += (*ncit);
	}
      else
	nodes_used.push_back(available_node);
      ++ncit;
    }

  //if the node was a vgige and we can't map any of unmapped neighbors with it than don't count this as a feasible cluster
  if (node->type == "V" && nodes_used.size() == 1 && lneighbors.size() > 0) 
    {  
      cout << "compute_mapping_cost reject cluster " << cluster->in << " node:" << node->label << " can't map any neighbors on cluster" << endl;
      return -1;
    }

  //TO DO deal with possible split cost
  //if we're mapping a vswitch and have more than one unmapped nodes
  //calculate the cost of splitting this switch with everything leftover placed on a single vswitch
  if (node->type == "V" && unmapped_nodes.size() > 1)
    {
      //see if there is a cluster that we can map the split switch to with all of its leaves 
      std::list<node_resource_ptr> vgige_nodes;
      link_resource vgige_lnk;
      vgige_lnk.node1 = node;
      vgige_lnk.node2 = node;
      vgige_lnk.rload = 0;
      vgige_lnk.lload = 0;
      bool can_split = false;

      //first calculate the loads for the new link created between the split vgige parts
      for (lit = node->links.begin(); lit != node->links.end(); ++lit)
	{
	  if ((*lit)->node1 == node)
	    {
	      if (in_list((*lit)->node2, unmapped_nodes)) 
		{
		  vgige_lnk.rload += (*lit)->lload;
		  if ((*lit)->node2->type != "V") vgige_nodes.push_back((*lit)->node2);
		}
	      else vgige_lnk.lload += (*lit)->rload;
	    }
	  else
	    {
	      if (in_list((*lit)->node1, unmapped_nodes))
		{
		  vgige_lnk.rload += (*lit)->rload;
		  if ((*lit)->node1->type != "V") vgige_nodes.push_back((*lit)->node1);
		}
	      else vgige_lnk.lload += (*lit)->lload;
	    }
	}
      //if the new link requires less than or equal the intercluster capacity, 
      //see if we can find a cluster that will accommodate the new switch and it's neighbors
      if ((vgige_lnk.rload <= MAX_INTERCLUSTER_CAPACITY) && (vgige_lnk.lload <= MAX_INTERCLUSTER_CAPACITY))
	{
	  node_resource_ptr potential_cluster;
	  int potential_cost = -1;
	  for (clusterit = base->nodes.begin(); clusterit != base->nodes.end(); ++clusterit)
	    {
	      if ((*clusterit)->type == "I" && (*clusterit) != cluster)
		{
		  nodes_used.clear();
		  bool failed = false;
		  for (reqnit = vgige_nodes.begin(); reqnit != vgige_nodes.end(); ++reqnit)
		    {
		      node_resource_ptr avail_node = find_available_node(*clusterit, (*reqnit)->type, nodes_used);
		      if (!avail_node)
			{
			  failed = true;
			  break;
			}
		    }
		  if (!failed)
		    {
		      //std::list<link_resource_ptr> potential_path;
		      link_resource_ptr potential_path(new link_resource());
		      potential_path->node1 = cluster;
		      potential_path->node1_port = -1;
		      potential_path->node2 = (*clusterit);
		      potential_path->node2_port = -1;
		      link_resource_ptr vlnk(&vgige_lnk);
		      int pcost = find_cheapest_path_breadth(vlnk, potential_path); 
		      if (pcost > 0 && (pcost < potential_cost || !potential_cluster))
			{
			  potential_cluster = (*clusterit);
			  potential_cost = pcost;
			} 
		    }
		}
	    }
	  if (potential_cluster) 
	    {
	      cluster_cost += potential_cost;
	      can_split = true;
	    }
	}

      //the split isn't going to work so impose a penalty and return cost	
      if (!can_split) cluster_cost += CANT_SPLIT_VGIGE_COST;
    }

  return cluster_cost;
}
/*
int
reservations::find_cheapest_path(link_resource_ptr ulink, link_resource_ptr potential_path) throw()
{
  std::list<node_resource_ptr> nodes_seen;
  nodes_seen.push_back(source);
  cout << "reservations::find_cheapest_path (" << source->label << "," << sink->label << ")(";
  //int rtn = find_cheapest_path(source, src_port, sink, sink_port, ulink, potential_path, nodes_seen);
  int rtn = find_cheapest_path_breadth(source, src_port, sink, sink_port, ulink, potential_path);
  cout << ")" << std::endl;
  return rtn;
}

int
reservations::find_cheapest_path(node_resource_ptr source, int src_port, node_resource_ptr sink, int sink_port, link_resource_ptr ulink, std::list<link_resource_ptr> potential_path, std::list<node_resource_ptr> nodes_seen) throw()
{
  int current_cost = -1;
  int cost = 0;
  std::list<link_resource_ptr> best_path;
  std::list<link_resource_ptr>::iterator lit;
  node_resource_ptr other_node;
  int o_port = -1;
  bool is_right;
  bool is_inlink;
  cout << "find(" << source->label << "," << sink->label << ")(";
  for (lit = source->links.begin(); lit != source->links.end(); ++lit)
    {
      o_port = -1;
      is_right = true;
      is_inlink = false;
      if ((*lit)->in > 0) is_inlink = true;
      if (((*lit)->node1 == source) && (src_port < 0 || (*lit)->node1_port == src_port))
	{
	  other_node = (*lit)->node2;
	  if (other_node->type != "I") o_port = (*lit)->node2_port;
	}
      else if (((*lit)->node2 == source) && (src_port < 0 || (*lit)->node2_port == src_port))
	{
	  other_node = (*lit)->node1;
	  if (other_node->type != "I") o_port = (*lit)->node1_port;
	  is_right = false;
	}
      else 
	{
	  cout << " skipping link:(" << (*lit)->node1->label << "p" <<  (*lit)->node1_port << "," << (*lit)->node2->label << "p" <<  (*lit)->node1_port << ")";
	  continue; //this is a link for some other port on this node
	}
      cout << " next:" << other_node->label;
      //have we already seen and processed this node
      std::list<node_resource_ptr>::iterator nit;
      bool seen = in_list(other_node, nodes_seen);
      
      //if there isn't enough capacity on this link, skip this link
      if (is_inlink && 
	  ((is_right && (((*lit)->potential_rcap < ulink->rload) || ((*lit)->potential_lcap < ulink->lload))) ||
	   (!is_right && (((*lit)->potential_lcap < ulink->rload) || ((*lit)->potential_rcap < ulink->lload)))))
	continue;
      if (other_node == sink && (o_port < 0 || o_port == sink_port))
	{
	  if (!is_inlink ||
	      (is_right && (((*lit)->potential_rcap >= ulink->rload) && ((*lit)->potential_lcap >= ulink->lload))) ||
	      (!is_right && (((*lit)->potential_lcap >= ulink->rload) && ((*lit)->potential_rcap >= ulink->lload))))
	    {
	       cout << " ,found sink";
	      cost = ulink->cost;
	      if (current_cost < 0 || cost < current_cost)
		{
		  best_path.clear();
		  best_path.push_back(*lit);
		  current_cost = cost;
		}
	    }
	}
      else if (seen || (other_node->type != "I")) continue;
      else
	{
	  std::list<link_resource_ptr> ppath;
	  std::list<node_resource_ptr> nodes_seen2;
	  nodes_seen2.assign(nodes_seen.begin(), nodes_seen.end());
	  nodes_seen2.push_back(other_node);
	  cost = find_cheapest_path(other_node, o_port, sink, sink_port, ulink, ppath, nodes_seen2);
	  if (cost >= 0 && (current_cost < 0 || cost < current_cost))
	    {
	      best_path.clear();
	      best_path.assign(ppath.begin(), ppath.end());
	      best_path.push_front(*lit);
	      current_cost = cost;
	    }
	}
    }
  if (current_cost >= 0)
    {
      while(!best_path.empty())
	{
	  potential_path.push_back(*(best_path.begin()));
	  best_path.pop_front();
	}
    }
  cout << ")";
  return current_cost;
}

*/
int
reservations::find_cheapest_path_breadth(link_resource_ptr ulink, link_resource_ptr potential_path) throw()
{
  int current_cost = -1;
  std::list<link_resource_ptr> best_path;
  std::list<link_resource_ptr>::iterator lit;
  bool is_right = true;

  node_resource_ptr source = potential_path->node1;
  int src_port = potential_path->node1_port;
  node_resource_ptr sink = potential_path->node2;
  int sink_port = potential_path->node2_port;

  std::list<link_path_ptr> current_paths;
  std::list<link_path_ptr> new_paths;

  std::list<node_resource_ptr> nodes_seen;
  nodes_seen.push_back(source);
  std::list<link_path_ptr>::iterator pathit;

  for (lit = source->links.begin(); lit != source->links.end(); ++lit)
    {
      node_resource_ptr othernode;
      int o_port = -1;
      if ((*lit)->node1 == source && (src_port < 0 || src_port == (*lit)->node1_port))
	{
	  othernode = (*lit)->node2;
	  if (othernode->type != "I")
	    o_port = (*lit)->node2_port;
	  is_right = true;
	}
      else if ((*lit)->node2 == source && (src_port < 0 || src_port == (*lit)->node2_port))
	{
	  othernode = (*lit)->node1;
	  if (othernode->type != "I")
	    o_port = (*lit)->node1_port;
	  is_right = false;
	}
      if (othernode)
	{
	  if ((*lit)->in > 0 && 
	      ((is_right && (((*lit)->potential_rcap < ulink->rload) || ((*lit)->potential_lcap < ulink->lload))) ||
	       (!is_right && (((*lit)->potential_lcap < ulink->rload) || ((*lit)->potential_rcap < ulink->lload)))))
		continue;
	  link_path_ptr tmp_path(new link_path());
	  tmp_path->path.push_back(*lit);
	  tmp_path->sink = othernode;
	  tmp_path->sink_port = o_port;
	  if ((*lit)->in > 0) tmp_path->cost = ulink->cost;
	  else tmp_path->cost = 0;	    
	  current_paths.push_back(tmp_path);
	}
    }
  while(!current_paths.empty())
    {
      new_paths.clear();
      for (pathit = current_paths.begin(); pathit != current_paths.end(); ++pathit)
	{
	  node_resource_ptr psnk = (*pathit)->sink;
	  int psnk_port = (*pathit)->sink_port;
	  if (!in_list(psnk, nodes_seen)) nodes_seen.push_back(psnk);
	  if (psnk == sink && (sink_port < 0 || psnk_port == sink_port))
	    {
	      if (current_cost < 0 || current_cost > ((*pathit)->cost))
		{
		  current_cost = (*pathit)->cost;
		  best_path.clear();
		  best_path.assign((*pathit)->path.begin(), (*pathit)->path.end());
		}
	    }
	  else if (psnk->type == "I") //stop if the node is not an infrastructure node but a leaf
	    {
	      for (lit = psnk->links.begin(); lit != psnk->links.end(); ++lit)
		{
		  node_resource_ptr othernode;
		  int o_port = -1;
		  if ((*lit)->node1 == psnk && (psnk_port < 0 || psnk_port == (*lit)->node1_port))
		    {
		      othernode = (*lit)->node2;
		      if (othernode->type != "I")
			o_port = (*lit)->node2_port;
		      is_right = true;
		    }
		  else if ((*lit)->node2 == psnk && (psnk_port < 0 || psnk_port == (*lit)->node2_port))
		    {
		      othernode = (*lit)->node1;
		      if (othernode->type != "I")
			o_port = (*lit)->node1_port;
		      is_right = false;
		    }
		  if (othernode && (!in_list(othernode, nodes_seen) || othernode == sink))
		    {
		      if ((*lit)->in > 0 && 
			  ((is_right && (((*lit)->potential_rcap < ulink->rload) || ((*lit)->potential_lcap < ulink->lload))) ||
			   (!is_right && (((*lit)->potential_lcap < ulink->rload) || ((*lit)->potential_rcap < ulink->lload)))))
			continue;
		      link_path_ptr tmp_path(new link_path());
                      tmp_path->path.assign((*pathit)->path.begin(), (*pathit)->path.end());
		      tmp_path->path.push_back(*lit);
		      tmp_path->sink = othernode;
		      tmp_path->sink_port = o_port;
		      if ((*lit)->in > 0) tmp_path->cost = ((*pathit)->cost) + (ulink->cost);
		      else tmp_path->cost = ((*pathit)->cost);	    
		      new_paths.push_back(tmp_path);
		    }
		}
	    }
	}
      current_paths.clear();
      if (current_cost >= 0)
	{
	  potential_path->cost = current_cost;
	  while(!best_path.empty())
	    {
	      potential_path->mapped_path.push_back(*(best_path.begin()));
	      best_path.pop_front();
	    }
	}
      else
	{
	  current_paths.assign(new_paths.begin(), new_paths.end());
	}
    }
  return current_cost;
}


void 
reservations::calculate_node_costs(topology* req) throw()
{
  //req->calculate_subnets();
  calculate_edge_loads(req);
  //merge vgige's
  std::list<node_resource_ptr>::iterator reqnit;
  std::list<link_resource_ptr>::iterator reqlit;
  for (reqnit = req->nodes.begin(); reqnit != req->nodes.end(); ++reqnit)
    {
      int cost = 0;
      for(reqlit = (*reqnit)->links.begin(); reqlit != (*reqnit)->links.end(); ++reqlit)
	cost += (*reqlit)->cost;
      (*reqnit)->cost = cost;
    }
  
}

void
reservations::calculate_edge_loads(topology* req) throw()
{
  std::list<node_resource_ptr>::iterator reqnit;
  std::list<link_resource_ptr>::iterator reqlit;
  std::list<link_resource_ptr> links_seen;
  int load = 0;
  
  for (reqnit = req->nodes.begin(); reqnit != req->nodes.end(); ++reqnit)
    {
      if ((*reqnit)->type != "V") 
	{
	  links_seen.clear();
	  //cycle through edges for each node in edge calculate the load generated from each side
	  for (reqlit = (*reqnit)->links.begin(); reqlit != (*reqnit)->links.end(); ++reqlit)
	    {
	      load = (*reqlit)->capacity;
	      if ((*reqlit)->node1 == (*reqnit))
		add_edge_load((*reqlit)->node1, (*reqlit)->node1_port, load, links_seen);
	      else
		add_edge_load((*reqlit)->node2, (*reqlit)->node2_port, load, links_seen);
	    }
	}
    }
  //now calculate the edge costs
  for (reqlit = req->links.begin(); reqlit != req->links.end(); ++reqlit)
    {
      (*reqlit)->cost = calculate_edge_cost((*reqlit)->rload, (*reqlit)->lload);
    }
}

void
reservations::add_edge_load(node_resource_ptr node, int port, int load, std::list<link_resource_ptr> links_seen) throw()
{
  std::list<link_resource_ptr>::iterator nlit;
  std::list<link_resource_ptr>::iterator lslit;
  bool is_seen = false;
  bool is_vgige = false;
  if (node->type == "V") is_vgige = true;

  for (nlit = node->links.begin(); nlit != node->links.end(); ++nlit)
    {
      is_seen = false;
      for (lslit = links_seen.begin(); lslit != links_seen.end(); ++lslit)
	{
	  if (*nlit == *lslit)
	    { 
	      is_seen = true;
	      break;
	    }
	}
      if (is_seen) continue;
      if ((*nlit)->node1 == node && ((*nlit)->node1_port == port || is_vgige))
	{
	  links_seen.push_back(*nlit);
	  (*nlit)->rload += load;
	  add_edge_load((*nlit)->node2, (*nlit)->node2_port, load, links_seen);
	}
      else if ((*nlit)->node2 == node && ((*nlit)->node2_port == port || is_vgige))
	{
	  links_seen.push_back(*nlit);
	  (*nlit)->lload += load;
	  add_edge_load((*nlit)->node1, (*nlit)->node1_port, load, links_seen);
	}
    }
}

int reservations::make_reservation(std::string begin1, std::string begin2, unsigned int rlen, topology *t) throw(reservation_exception)
{
  unsigned int horizon = 10;
  unsigned int divisor = 4;

  time_t current_time_unix;
  time_t begin1_unix;
  time_t begin2_unix;
  time_t end1_unix;
  time_t end2_unix;

  list<node_resource_ptr>::iterator node;
  list<link_resource_ptr>::iterator link;

  current_time_unix = time(NULL);
  current_time_unix = discretize_time(current_time_unix, divisor);

  begin1_unix = time_str2unix(begin1);
  begin1_unix = discretize_time(begin1_unix, divisor);

  begin2_unix = time_str2unix(begin2);
  begin2_unix = discretize_time(begin2_unix, divisor);

  if(begin1_unix > begin2_unix)
  {
    begin2_unix = begin1_unix;
  }
  
  if(begin1_unix < current_time_unix && begin2_unix < current_time_unix)
  {
    throw reservation_exception((std::string)"time range is in the past");
  }
  
  if(begin1_unix < current_time_unix)
  {
    begin1_unix = current_time_unix;
  }

  if(rlen <= 0)
  {
    throw reservation_exception((std::string)"length must be positive");
  }
 
  unsigned int chunk = 60/divisor;
  unsigned int len = (((rlen-1) / chunk)+1) * chunk;

  end1_unix = add_time(begin1_unix, len*60);
  end2_unix = add_time(begin2_unix, len*60);

  if(t->nodes.size() == 0)
  {
    throw reservation_exception((std::string)"topology has no nodes");
  }

  time_t horizon_limit_unix = add_time(current_time_unix, horizon*24*60*60);
  if(end1_unix > horizon_limit_unix)
  {
    throw reservation_exception((std::string)"time too far into the future");
  }
 
  if(end2_unix > horizon_limit_unix)
  {
    end2_unix = horizon_limit_unix;
    begin2_unix = sub_time(end2_unix, len*60);
  }
  std::list<time_t> times_of_interest;

  std::list<schedule_entry_ptr>::iterator s;
  for(s=schedule.begin(); s!=schedule.end(); ++s)
  {
    if(((*s)->b_unix < end2_unix) && ((*s)->e_unix > begin1_unix))
    {
      if(((*s)->b_unix > begin1_unix) && ((*s)->b_unix < end2_unix))
      { 
        times_of_interest.push_back((*s)->b_unix);
      }
      if(((*s)->e_unix > begin1_unix) && ((*s)->e_unix < end2_unix))
      { 
        times_of_interest.push_back((*s)->e_unix);
      }
    }
  }

  times_of_interest.sort();
  times_of_interest.unique();

  std::list<time_t>::iterator toi_start = times_of_interest.begin();
  std::list<time_t>::iterator toi_end = times_of_interest.begin();
  while(toi_end != times_of_interest.end() && *toi_end < end1_unix) ++toi_end;
 
  time_t cur_start, cur_end;
  unsigned int increment = (60/divisor)*60;
  bool changed = true;
  bool mip_has_run = false;
  for(cur_start = begin1_unix, cur_end = end1_unix; cur_start <= begin2_unix; cur_start = add_time(cur_start, increment), cur_end = add_time(cur_end, increment))
  {
    if(toi_start != times_of_interest.end() && *toi_start < cur_start)
    {
      changed = true;
      ++toi_start;
    }
    if(toi_end != times_of_interest.end() && *toi_end < cur_end)
    {
      changed = true;
      ++toi_end;
    }
    if(changed)
    {
      int resres = try_reservation(t, cur_start, cur_end);
      if(resres == 2) 
	{
	  cout << "RESERVATION START: " << cur_start << endl;
	  return 2;
	}
      if(resres == 0) mip_has_run = true;
      changed = false;
    }
  }

  if(mip_has_run) return 0;
  return 1;
}

void
reservations::unmap_reservation(topology* req) throw()
{
  std::list<node_resource_ptr>::iterator nit;
  std::list<link_resource_ptr>::iterator lit;
  std::list<link_resource_ptr>::iterator mplit;
  node_resource_ptr nullnode;
  
  for(nit = req->nodes.begin(); nit != req->nodes.end(); ++nit)
    {
      if ((*nit)->is_mapped)
	{
	  (*nit)->is_mapped = false;
	  //(*nit)->marked = false;
	  (*nit)->in = 0;
	  (*nit)->mapped_node->is_mapped = false;
	  (*nit)->mapped_node = nullnode;
	}
    }

  for(lit = req->links.begin(); lit != req->links.end(); ++lit)
    {
      if ((*lit)->is_mapped)
	{
	  (*lit)->is_mapped = false;
	  node_resource_ptr lastnode = (*lit)->node1->mapped_node;
	  for(mplit = (*lit)->mapped_path.begin(); mplit != (*lit)->mapped_path.end(); ++mplit)
	    {
	      if ((*mplit)->node1 == lastnode)
		{
		  if ((*mplit)->in == 0)
		    {
		      (*mplit)->rload -= (*lit)->rload;
		      (*mplit)->lload -= (*lit)->lload;
		    }
		  lastnode = (*mplit)->node2;
		}
	      else //using link int the other direction
		{
		  if ((*mplit)->in == 0)
		    {
		      (*mplit)->rload -= (*lit)->lload;
		      (*mplit)->lload -= (*lit)->rload;
		    }
		  lastnode = (*mplit)->node1;
		}
	    }
	  (*lit)->mapped_path.clear();
	}
    }
}


int
reservations::compute_host_cost(topology* topo)
{
  std::list<node_resource_ptr>::iterator nit;
  std::list<link_resource_ptr>::iterator lit;
  int rtn = 0;
  for (nit = topo->nodes.begin(); nit != topo->nodes.end(); ++nit)
    {
      if (!((*nit)->type == "V" || (*nit)->type == "X")) //it's not a vgige or an ixp
	{
	  for(lit = (*nit)->links.begin(); lit != (*nit)->links.end(); ++lit)
	    {
	      rtn += (*lit)->capacity;
	      break;
	    }
	}
    }
  return (2*rtn);
}


int
reservations::compute_intercluster_cost(topology* topo)
{
  int rtn = 0;
  std::list<link_resource_ptr>::iterator lit;
  std::list<link_resource_ptr>::iterator plit;

  for (lit = topo->links.begin(); lit != topo->links.end(); ++lit)
    {
      if ((*lit)->is_mapped)
	{
	  for (plit = (*lit)->mapped_path.begin(); plit != (*lit)->mapped_path.end(); ++plit)
	    {
	      if ((*plit)->in > 0)
		rtn += (*lit)->cost;
	    }
	}
    }
  return rtn;
}
