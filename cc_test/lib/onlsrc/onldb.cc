#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <list>
#include "time.h"
#include <stdio.h>
#include <stdlib.h>

#include <mysql++/mysql++.h>
#include <mysql++/ssqls.h>
#include <boost/shared_ptr.hpp>

#include <gurobi_c++.h>

#include "internal.h"
#include "onldb_resp.h"
#include "topology.h"
#include "onldb_internal.h"
#include "onldb.h"
#include "onldb_types.h"

using namespace std;
using namespace onl;

// better make sure that all binaries are not globally readable!!
#define ONLDB     "onlnew"
#define ONLDBHOST "localhost"
#define ONLDBUSER "onladmin"
#define ONLDBPASS "onlrocks!"

bool req_sort_comp(assign_info_ptr i, assign_info_ptr j)
{
  return(i->user_nodes.size() < j->user_nodes.size());
}

bool base_sort_comp(node_resource_ptr i, node_resource_ptr j)
{
  return(i->priority < j->priority);
}

bool onldb::lock(std::string l) throw()
{
  try
  {
    mysqlpp::Query lock = onl->query();
    lock << "select get_lock(" << mysqlpp::quote << l << ",10) as lockres";
    mysqlpp::StoreQueryResult res = lock.store();
    lockresult lr = res[0];
    if(lr.lockres.is_null || lr.lockres.data == 0) return false;
  } 
  catch(const mysqlpp::Exception& er)
  {
    return false;
  }
  return true;
}

void onldb::unlock(std::string l) throw()
{
  try
  {
    mysqlpp::Query lock = onl->query();
    lock << "do release_lock(" << mysqlpp::quote << l <<")";
    lock.execute();
  } 
  catch(const mysqlpp::Exception& er)
  {
    cout << "Warning: releasing lock " << l << " encountered an exception" << endl;
  }
}

onldb::onldb() throw()
{
  onl = new mysqlpp::Connection(ONLDB,ONLDBHOST,ONLDBUSER,ONLDBPASS);

  nodestates::table("nodes");
  restimes::table("reservations");
  resinfo::table("reservations");
  experimentins::table("experiments");
  reservationins::table("reservations");
}

onldb::~onldb() throw()
{
  delete onl;
}

std::string onldb::time_unix2db(time_t unix_time) throw()
{
  struct tm *stm = localtime(&unix_time);

  char char_str[16];
  sprintf(char_str,"%04d%02d%02d%02d%02d%02d",stm->tm_year+1900,stm->tm_mon+1,stm->tm_mday,stm->tm_hour,stm->tm_min,stm->tm_sec);
  std::string str = char_str;
  return str;
}

int onldb::str2int(std::string s) throw()
{
  stringstream ss(s);
  int n;
  ss >> n;
  return n;
}

time_t onldb::time_db2unix(std::string db_time) throw()
{
  struct tm stm;

  if(db_time.length() == 14)
  {
    // YYYYMMDDhhmmss
    stm.tm_sec  = str2int(db_time.substr(12,2));
    stm.tm_min  = str2int(db_time.substr(10,2));
    stm.tm_hour = str2int(db_time.substr(8,2));
    stm.tm_mday = str2int(db_time.substr(6,2));
    stm.tm_mon  = str2int(db_time.substr(4,2)) - 1;
    stm.tm_year = str2int(db_time.substr(0,4)) - 1900;
  }
  else
  {
    // YYYY-MM-DD hh:mm:ss
    stm.tm_sec  = str2int(db_time.substr(17,2));
    stm.tm_min  = str2int(db_time.substr(14,2));
    stm.tm_hour = str2int(db_time.substr(11,2));
    stm.tm_mday = str2int(db_time.substr(8,2));
    stm.tm_mon  = str2int(db_time.substr(5,2)) - 1;
    stm.tm_year = str2int(db_time.substr(0,4)) - 1900;
  }

  stm.tm_isdst = -1;
  return mktime(&stm);
}

time_t onldb::get_start_of_week(time_t time) throw()
{
  struct tm *stm = localtime(&time);

  stm->tm_hour = 0;
  stm->tm_min = 0;
  stm->tm_sec = 0;
  // substract off days so that we are on the previous monday. mktime
  // does the correct thing with negative values, potentially changing 
  // month and year.
  if(stm->tm_wday == 0) // 0 is Sunday, but we want Monday->Sunday weeks
  {
    stm->tm_mday -= 6;
  }
  else
  {
    stm->tm_mday -= (stm->tm_wday - 1);
  }

  return mktime(stm);
}

time_t onldb::discretize_time(time_t unix_time, unsigned int hour_divisor) throw()
{
  struct tm *stm = localtime(&unix_time);
  unsigned int time_chunk = 60/hour_divisor;
  stm->tm_sec = 0;
  stm->tm_min = (stm->tm_min / time_chunk) * time_chunk;
  return mktime(stm);
}

time_t onldb::add_time(time_t unix_time, unsigned int seconds) throw()
{
  struct tm *stm = localtime(&unix_time);
  stm->tm_sec += seconds;
  return mktime(stm);
} 

time_t onldb::sub_time(time_t unix_time, unsigned int seconds) throw()
{
  struct tm *stm = localtime(&unix_time);
  stm->tm_sec -= seconds;
  return mktime(stm);
} 

std::string onldb::get_type_type(std::string type) throw()
{
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select type from types where tid=" << mysqlpp::quote << type;
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty()) return "";

    typetype tt = res[0];
    return tt.type;
  }
  catch(const mysqlpp::Exception& er)
  {
    return "";
  }
}

onldb_resp onldb::is_infrastructure(std::string node) throw()
{
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select type from types where tid in (select tid from nodes where node=" << mysqlpp::quote << node << ")";
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty()) return onldb_resp(-1,(std::string)"node not in database");

    typetype tt = res[0];
    if(tt.type == "infrastructure")
    {
      return onldb_resp(1,(std::string)"true");
    }
    return onldb_resp(0,(std::string)"false");
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,(std::string)"database problem");
  }
}

onldb_resp onldb::verify_clusters(topology *t) throw()
{
  list<node_resource_ptr>::iterator nit;
  list<link_resource_ptr>::iterator lit;

  // set up the marked fields for all components
  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    if((*nit)->parent)
    {
      (*nit)->marked = false;
    }
    else
    {
      (*nit)->marked = true;
    }
  }

  for(lit = t->links.begin(); lit != t->links.end(); ++lit)
  {
    (*lit)->marked = true;
  }
  
  // fill in the type_type field for all nodes
  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    (*nit)->type_type = get_type_type((*nit)->type);
    if((*nit)->type_type == "")
    {
      std::string err = (*nit)->type + " is not a valid type";
      return onldb_resp(-1,err);
    }
  }

  // abstract_tops contains one abstract cluster topology for each cluster in the actual topology
  vector<topology_resource_ptr> abstract_tops;

  // make a pass over the node list, only need to match types to types
  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    // if the node is already marked, then skip it
    if((*nit)->marked) { continue; }

    unsigned int pl = (*nit)->parent->label;
    std::string pt = (*nit)->parent->type;
    std::string ptt = (*nit)->parent->type_type;

    // if the node's parent is not a cluster type, it is not valid
    if(ptt != "hwcluster")
    {
      return onldb_resp(-1,(std::string)"node " + to_string(pl) + " is not a valid cluster type");
    }

    // now go through the list of abstract topologies to find the one this node belongs to
    topology_resource_ptr abs_top;
    vector<topology_resource_ptr>::iterator topit;
    for(topit = abstract_tops.begin(); topit != abstract_tops.end(); ++topit)
    {
      if((*topit)->label == pl) 
      {
        abs_top = *topit;
        break;
      }
    }
    // if there isn't yet an abstract topology for this cluster, add it
    if(topit == abstract_tops.end())
    {
      topology_resource_ptr trp(new topology_resource());
      trp->label = pl;
      
      // build the cluster from the cluster description in the database
      try
      {
        mysqlpp::Query query = onl->query();
        query << "select compid,comptype as type from clustercomps where clustercomps.tid=" << mysqlpp::quote << pt;
        vector<clusterelems> ces;
        query.storein(ces);
        if(ces.empty())
        {
          return onldb_resp(-1, (std::string)"database consistency problem");
        }

        vector<clusterelems>::iterator ceit;
        for(ceit = ces.begin(); ceit != ces.end(); ++ceit)
        {
          onldb_resp ahrr = trp->cluster.add_node(ceit->type, ceit->compid, 0);
          if(ahrr.result() != 1)
          {
            return onldb_resp(-1, (std::string)"database consistency problem");
          }
        }
      }
      catch(const mysqlpp::Exception& er)
      {
        return onldb_resp(-1,er.what());
      }
      
      abstract_tops.push_back(trp);
      abs_top = trp;
    }

    // now we have the abstract topology, so check if the current node is valid
    list<node_resource_ptr>::iterator atnit;
    for(atnit = abs_top->cluster.nodes.begin(); atnit != abs_top->cluster.nodes.end(); ++atnit)
    {
      if((*atnit)->marked) { continue; }
      if((*atnit)->type == (*nit)->type)
      {
        (*atnit)->marked = true;
        (*nit)->marked = true;
        break;
      }
    }
    if(!((*nit)->marked))
    {  
      return onldb_resp(-1,(std::string)"node " + to_string((*nit)->label) + " does not fit into cluster type " + pt);
    }
  }

  // now, all components in the topology are part of good clusters, but are the clusters complete?
  vector<topology_resource_ptr>::iterator at;
  for(at = abstract_tops.begin(); at != abstract_tops.end(); ++at)
  {
    list<node_resource_ptr>::iterator atnit;
    for(atnit = (*at)->cluster.nodes.begin(); atnit != (*at)->cluster.nodes.end(); ++atnit)
    {
      if((*atnit)->marked == false) 
      {
        return onldb_resp(-1,(std::string)"cluster " + to_string((*at)->label) + " is incomplete");
      }
    }
  }
  
  return onldb_resp(1,"success");
}

bool onldb::add_link(topology* t, int rid, unsigned int cur_link, unsigned int linkid, unsigned int cur_cap, unsigned int node1_label, unsigned int node1_port, unsigned int node2_label, unsigned int node2_port) throw()
{
  // there are three cases to deal with: node->node (normal) links, node->vswitch
  // links, and vswitch->vswitch links. for node->node links, both node1_label
  // and node2_label should have been set.  for node->vswitch links, only
  // node1_label is set.  for vswitch->vswitch links, neither lable has been set.
  
  // if node2_label isn't set, then this link involves at least one vswitch, so
  // read the vswitchschedule table to get all vswitches that are part of this link
  if(node2_label == 0)
  {
    unsigned int num_vs = 1;
    if(node1_label == 0) { num_vs++; }
  
    // note that we assume that vswitches can NOT have loopbacks
    mysqlpp::Query vswitchquery = onl->query();
    vswitchquery << "select vlanid,port from vswitchschedule where rid=" << mysqlpp::quote << rid << " and linkid=" << mysqlpp::quote << cur_link; 
    vector<vswitchconns> vswc;
    vswitchquery.storein(vswc);
    if(vswc.size() != num_vs) { return false; }

    if(node1_label == 0)
    {
      node1_label = t->get_label("vgige" + to_string(vswc[1].vlanid));
      node1_port = vswc[1].port;
    }
    node2_label = t->get_label("vgige" + to_string(vswc[0].vlanid));
    node2_port = vswc[0].port;
  }

  onldb_resp r = t->add_link(linkid, cur_cap, node1_label, node1_port, node2_label, node2_port);
  if(r.result() != 1) { return false; }

  return true;
}

onldb_resp onldb::get_topology(topology *t, int rid) throw()
{
  unsigned hwid = 1;
  unsigned linkid = 1;
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select hwclusterschedule.cluster,hwclusters.tid,hwclusters.acl,hwclusterschedule.fixed from hwclusters join hwclusterschedule using (cluster) where hwclusterschedule.rid=" << mysqlpp::quote << rid;
    vector<hwclustertypes> hwct;
    query.storein(hwct);
    vector<hwclustertypes>::iterator it;
    for(it = hwct.begin(); it != hwct.end(); ++it)
    { 
      onldb_resp r = t->add_node(it->tid, hwid, 0);
      if(r.result() != 1)
      {
        return onldb_resp(-1, (std::string)"database consistency problem");
      }
      ++hwid;
      t->nodes.back()->node = it->cluster;
      t->nodes.back()->acl = it->acl;
      t->nodes.back()->cp = "unused";
      if(it->fixed == 1)
      {
        t->nodes.back()->fixed = true;
      }
    }

    mysqlpp::Query queryvlan = onl->query();
    queryvlan << "select distinct vlanid from vswitchschedule where rid=" << mysqlpp::quote << rid;
    vector<vswitches> vsw;
    queryvlan.storein(vsw);
    vector<vswitches>::iterator itv;
    for(itv = vsw.begin(); itv != vsw.end(); ++itv)
    {
      onldb_resp r = t->add_node("vgige", hwid, 0);
      if(r.result() != 1)
      {
        return onldb_resp(-1, (std::string)"database consistency problem");
      }
      ++hwid;
      t->nodes.back()->node = "vgige" + to_string(itv->vlanid);
      t->nodes.back()->acl = "unused";
      t->nodes.back()->cp = "unused";
    }

    mysqlpp::Query query2 = onl->query();
    query2 << "select nodes.node,nodes.tid,hwclustercomps.cluster,nodes.acl,nodes.daemonhost,nodeschedule.fixed from nodes join nodeschedule using (node) left join hwclustercomps using (node) where nodeschedule.rid=" << mysqlpp::quote << rid;
    vector<nodetypes> nt;
    query2.storein(nt);
    vector<nodetypes>::iterator it2;
    for(it2 = nt.begin(); it2 != nt.end(); ++it2)
    {
      unsigned int parent_label = 0;
      if(!it2->cluster.is_null)
      {
        parent_label = t->get_label(it2->cluster.data);
        if(parent_label == 0) return onldb_resp(-1, (std::string)"database consistency problem");
      }
      onldb_resp r = t->add_node(it2->tid, hwid, parent_label);
      if(r.result() != 1)
      {
        return onldb_resp(-1, (std::string)"database consistency problem");
      }
      ++hwid;
      t->nodes.back()->node = it2->node;
      t->nodes.back()->acl = it2->acl;
      t->nodes.back()->cp = it2->daemonhost;
      if(it2->fixed == 1)
      {
        t->nodes.back()->fixed = true;
      }
    }

    mysqlpp::Query query3 = onl->query();
    query3 << "select connschedule.linkid,connschedule.capacity,connections.cid,connections.node1,connections.node1port,connections.node2,connections.node2port from connschedule join connections on connections.cid=connschedule.cid where connschedule.rid=" << mysqlpp::quote << rid << " order by connschedule.linkid";
    vector<linkinfo> li;
    query3.storein(li);
    vector<linkinfo>::iterator it3;
    if(!li.empty())
    {
      unsigned int cur_link = li.begin()->linkid;
      unsigned int cur_cap = li.begin()->capacity;
      std::list<int> cur_conns;
      unsigned int node1_label = 0;
      unsigned int node1_port;
      unsigned int node2_label = 0;
      unsigned int node2_port;
      for(it3 = li.begin(); it3 != li.end(); ++it3)
      {
        if(cur_link != it3->linkid)
        {
          if(!add_link(t, rid, cur_link, linkid, cur_cap, node1_label, node1_port, node2_label, node2_port))
          {
            return onldb_resp(-1, (std::string)"database consistency problem");
          }
          t->links.back()->conns = cur_conns;
          ++linkid;

          cur_link = it3->linkid;
          cur_cap = it3->capacity;
          cur_conns.clear();
          node1_label = 0;
          node2_label = 0;
        }
      
        cur_conns.push_back(it3->cid);

        // node 2 is always an infrastructure node, so just check node1
        onldb_resp r1 = is_infrastructure(it3->node1); 
        if(r1.result() < 0) return onldb_resp(-1, (std::string)"database consistency problem");
        if(r1.result() == 0)
        {
          if(node1_label == 0)
          {
            node1_label = t->get_label(it3->node1);
            node1_port = it3->node1port;
          }
          else if(node2_label == 0)
          {
            node2_label = t->get_label(it3->node1);
            node2_port = it3->node1port;
          }
          else
          {
            return onldb_resp(-1, (std::string)"database consistency problem");
          }
        }
      }
      if(!li.empty())
      {
        if(!add_link(t, rid, cur_link, linkid, cur_cap, node1_label, node1_port, node2_label, node2_port))
        {
          return onldb_resp(-1, (std::string)"database consistency problem");
        }
        t->links.back()->conns = cur_conns;
      }
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
  return onldb_resp(1, (std::string)"success");
}

void onldb::build_assign_list(node_resource_ptr hw, std::list<assign_info_ptr> *l) throw()
{
  std::list<link_resource_ptr>::iterator lit;
  std::list<assign_info_ptr>::iterator ait;

  if(hw->marked) return;
  hw->marked = true;
    
  for(ait = l->begin(); ait != l->end(); ++ait)
  {
    if((*ait)->type == hw->type)
    {
      (*ait)->user_nodes.push_back(hw);
      break;
    }
  }
  if(ait == l->end())
  {
    assign_info_ptr newnode(new assign_info());
    newnode->type = hw->type;
    newnode->user_nodes.push_back(hw);
    newnode->marked = false;
    l->push_back(newnode);
  }

  for(lit = hw->links.begin(); lit != hw->links.end(); ++lit)
  {
    if((*lit)->marked) continue;
    (*lit)->marked = true;
    node_resource_ptr other_end = (*lit)->node1;
    if((*lit)->node1->label == hw->label)
    {
      other_end = (*lit)->node2;
    }
    build_assign_list(other_end, l);
  }
  return;
}

onldb_resp onldb::fill_in_topology(topology *t, int rid) throw()
{
  std::list<node_resource_ptr>::iterator nit;
  std::list<link_resource_ptr>::iterator lit;

  // make sure that all the nodes and links are not marked before we start
  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    (*nit)->marked = false;
  }
  for(lit = t->links.begin(); lit != t->links.end(); ++lit)
  {
    (*lit)->marked = false;
  }

  //first create a list of the nodes separated by type, for each disconnected sub-topology
  std::list< std::list<assign_info_ptr>* > assign_lists;
  std::list< std::list<assign_info_ptr>* >::iterator assign_list;
  std::list<assign_info_ptr>::iterator ait;
  std::list<assign_info_ptr> *new_list;

  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    if((*nit)->is_parent) continue;
    if((*nit)->marked) continue;
    new_list = new std::list<assign_info_ptr>();
    build_assign_list(*nit, new_list);
    new_list->sort(req_sort_comp);
    assign_lists.push_back(new_list);
  }

  // next build the reserved topology and add its stuff to the assign_list
  topology res_top;
  onldb_resp r = get_topology(&res_top, rid);
  if(r.result() < 1)
  {
    while(!assign_lists.empty())
    {
      new_list = (std::list<assign_info_ptr> *)assign_lists.front();
      assign_lists.pop_front();
      delete new_list;
    }
    return onldb_resp(r.result(),r.msg());
  }

  std::list<assign_info_ptr> res_list;
  std::list<assign_info_ptr>::iterator rit;
 
  for(nit = res_top.nodes.begin(); nit != res_top.nodes.end(); ++nit)
  {
    if((*nit)->is_parent) continue;
    for(ait = res_list.begin(); ait != res_list.end(); ++ait)
    {
      if((*ait)->type == (*nit)->type)
      {
        (*ait)->testbed_nodes.push_back(*nit);
        break;
      }
    }
    if(ait == res_list.end())
    {
      assign_info_ptr newnode(new assign_info());
      newnode->type = (*nit)->type;
      newnode->testbed_nodes.push_back(*nit);
      res_list.push_back(newnode);
    }
  }

  // check that the the number of each requested type is <= the number reserved
  for(rit = res_list.begin(); rit != res_list.end(); ++rit)
  {
    unsigned int num_abs = 0;
    for(assign_list = assign_lists.begin(); assign_list != assign_lists.end(); ++assign_list)
    {
      for(ait = (*assign_list)->begin(); ait != (*assign_list)->end(); ++ait)
      {
        if((*ait)->type == (*rit)->type)
        {
          (*ait)->marked = true;
          num_abs += (*ait)->user_nodes.size();
        }
      }
    }
    if(num_abs > (*rit)->testbed_nodes.size())
    {
      std::string s = "you requested more " + (*rit)->type + "s than you reserved";
      while(!assign_lists.empty())
      {
        new_list = (std::list<assign_info_ptr> *)assign_lists.front();
        assign_lists.pop_front();
        delete new_list;
      }
      return onldb_resp(0,s);
    }
  }
  for(assign_list = assign_lists.begin(); assign_list != assign_lists.end(); ++assign_list)
  {
    for(ait = (*assign_list)->begin(); ait != (*assign_list)->end(); ++ait)
    {
      if((*ait)->marked == false)
      {
        std::string s = "you requested more " + (*ait)->type + "s than you reserved";
        while(!assign_lists.empty())
        {
          new_list = (std::list<assign_info_ptr> *)assign_lists.front();
          assign_lists.pop_front();
          delete new_list;
        }
        return onldb_resp(0,s);
      }
    }
  }
 
  // make sure everything is unmarked and cleared before starting to find an assignment
  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    (*nit)->marked = false;
    (*nit)->node = "";
    (*nit)->acl = "unused";
    (*nit)->cp = "unused";
    (*nit)->level = 0;
  }
  for(lit = t->links.begin(); lit != t->links.end(); ++lit)
  {
    (*lit)->marked = false;
    (*lit)->conns.clear();
    (*lit)->level = 0;
  }
  for(nit = res_top.nodes.begin(); nit != res_top.nodes.end(); ++nit)
  {
    (*nit)->marked = false;
    (*nit)->level = 0;
  }
  for(lit = res_top.links.begin(); lit != res_top.links.end(); ++lit)
  {
    (*lit)->marked = false;
    (*lit)->level = 0;
  }

  // recursively try to make the assigment from reserved topology to requested topology, one subset at a time
  assign_list = assign_lists.begin();
  bool success = false;
  if(subset_assign(res_list, assign_list, assign_lists.end(), 1))
  {
    success = true;
  }

  while(!assign_lists.empty())
  {
    new_list = (std::list<assign_info_ptr> *)assign_lists.front();
    assign_lists.pop_front();
    delete new_list;
  }

  if(success) return onldb_resp(1,(std::string)"success");
  return onldb_resp(0,(std::string)"the requested topology does not match what was reserved");
}  

bool onldb::subset_assign(std::list<assign_info_ptr> rl, std::list< std::list<assign_info_ptr>* >::iterator ali, std::list< std::list<assign_info_ptr>* >::iterator end, unsigned int level) throw()
{
  std::list<assign_info_ptr> al = **ali;
  std::list<assign_info_ptr>::iterator ai, clean;
  std::list<assign_info_ptr>::iterator ri;
  std::list< std::list<assign_info_ptr>* >::iterator alinew;

  if(ali == end) return true;
  alinew = ali;
  ++alinew;

  // for the abstract components with the fewest number of that type, try assigning each reserved component
  // to the first such abstract component
  ai = al.begin();
  for(ri = rl.begin(); ri != rl.end(); ++ri)
  {
    if((*ai)->type == (*ri)->type) break;
  }
  if(ri == rl.end()) return false;

  std::list<node_resource_ptr>::iterator res_nit;
  std::list<node_resource_ptr>::iterator nit;
  std::list<link_resource_ptr>::iterator lit;
  for(res_nit = (*ri)->testbed_nodes.begin(); res_nit != (*ri)->testbed_nodes.end(); ++res_nit)
  {
    if((*res_nit)->marked) continue;

    if(find_mapping(*((*ai)->user_nodes.begin()), *res_nit, level))
    {
      if(subset_assign(rl, alinew, end, level+1)) return true;
    }

    // have to go through and set the marked field to false for all components and links before next test
    for(clean = rl.begin(); clean != rl.end(); ++clean)
    {
      for(nit = (*clean)->testbed_nodes.begin(); nit != (*clean)->testbed_nodes.end(); ++nit)
      {
        if((*nit)->marked && (*nit)->level == level)
        { 
          (*nit)->marked = false;
          (*nit)->level = 0;
          if((*nit)->parent && (*nit)->parent->marked && (*nit)->parent->level == level)
          {
            (*nit)->parent->marked = false;
            (*nit)->parent->level = 0;
          }
          for(lit = (*nit)->links.begin(); lit != (*nit)->links.end(); ++lit)
          {
            if((*lit)->marked && (*lit)->level == level)
            { 
              (*lit)->marked = false;
              (*lit)->level = 0;
            }
          }
        }
      }
    }
    for(clean = al.begin(); clean != al.end(); ++clean)
    {
      for(nit = (*clean)->user_nodes.begin(); nit != (*clean)->user_nodes.end(); ++nit)
      {
        (*nit)->marked = false;
        (*nit)->node = "";
        (*nit)->acl = "unused";
        (*nit)->cp = "unused";
        if((*nit)->parent && (*nit)->parent->marked && (*nit)->parent->level == level)
        {
          (*nit)->parent->marked = false;
          (*nit)->parent->level = 0;
        }
        for(lit = (*nit)->links.begin(); lit != (*nit)->links.end(); ++lit)
        {
          (*lit)->marked = false;
          (*lit)->conns.clear();
        }
      }
    }
  }
  return false;
}

bool onldb::find_mapping(node_resource_ptr abs_node, node_resource_ptr res_node, unsigned int level) throw()
{
  std::list<link_resource_ptr>::iterator abs_lit;
  std::list<link_resource_ptr>::iterator res_lit;

  if(abs_node->marked) return true;
  if(res_node->marked) return false;

  if(abs_node->parent && abs_node->parent->marked && (abs_node->parent->node != res_node->parent->node)) return false;

  abs_node->marked = true;
  res_node->marked = true;
  res_node->level = level;
  abs_node->node = res_node->node;
  abs_node->acl = res_node->acl;
  abs_node->cp = res_node->cp;
  if(abs_node->parent && !abs_node->parent->marked) 
  {
    abs_node->parent->node = res_node->parent->node;
    abs_node->parent->marked = true;
    abs_node->parent->level = level;
    res_node->parent->marked = true;
    res_node->parent->level = level;
  }

  for(abs_lit = abs_node->links.begin(); abs_lit != abs_node->links.end(); ++abs_lit)
  {
    if((*abs_lit)->marked) continue;

    node_resource_ptr abs_other_end = (*abs_lit)->node1;
    unsigned int abs_this_port = (*abs_lit)->node2_port;
    unsigned int abs_other_port = (*abs_lit)->node1_port;
    bool abs_this_is_loopback = false;
    if((*abs_lit)->node1->label == (*abs_lit)->node2->label)
    {
      abs_this_is_loopback = true;
    }
    if((*abs_lit)->node1->label == abs_node->label)
    {
      abs_other_end = (*abs_lit)->node2;
      abs_this_port = (*abs_lit)->node1_port;
      abs_other_port = (*abs_lit)->node2_port;
    }
    
    for(res_lit = res_node->links.begin(); res_lit != res_node->links.end(); ++res_lit)
    {
      if((*res_lit)->marked) continue;
  
      node_resource_ptr res_other_end = (*res_lit)->node1;
      unsigned int res_this_port = (*res_lit)->node2_port;
      unsigned int res_other_port = (*res_lit)->node1_port;
      bool res_this_is_loopback = false;
      if((*res_lit)->node1->label == (*res_lit)->node2->label)
      {
        res_this_is_loopback = true;
      }
      if(abs_this_is_loopback != res_this_is_loopback) continue;
      // dealing with loopback links, if the ports don't line up, switch them.
      // if they still don't line up, then this isn't the right link
      if( (res_this_is_loopback && (abs_this_port != res_this_port)) ||
          (!res_this_is_loopback && ((*res_lit)->node1->label == res_node->label)) )
      {
        res_other_end = (*res_lit)->node2;
        res_this_port = (*res_lit)->node1_port;
        res_other_port = (*res_lit)->node2_port;
      }

      if(abs_this_port == res_this_port)
      {
        if(abs_other_end->type != res_other_end->type) return false;
        if(abs_other_port != res_other_port) return false;

        (*abs_lit)->marked = true;
        (*res_lit)->marked = true;
        (*res_lit)->level = level;
        (*abs_lit)->conns = (*res_lit)->conns;

        if(find_mapping(abs_other_end, res_other_end, level) == false) return false;
        break;
      }
    }
    if(res_lit == res_node->links.end()) return false;
  }

  return true;
}

onldb_resp onldb::get_base_topology(topology *t, std::string begin, std::string end) throw()
{
  unsigned hwid = 1;
  unsigned linkid = 1;
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select cluster,priority,tid from hwclusters where cluster not in (select cluster from hwclusterschedule where rid in (select rid from reservations where state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << end << " and end>" << mysqlpp::quote << begin << " )) order by rand()";
    vector<baseclusterinfo> bci;
    query.storein(bci);
    vector<baseclusterinfo>::iterator it;
    for(it = bci.begin(); it != bci.end(); ++it)
    { 
      onldb_resp r = t->add_node(it->tid, hwid, 0);
      if(r.result() != 1)
      {
        return onldb_resp(-1, (std::string)"database consistency problem");
      }
      ++hwid;
      t->nodes.back()->node = it->cluster;
      t->nodes.back()->priority = (int)it->priority;
    }

    mysqlpp::Query query2 = onl->query();
    query2 << "select nodes.node,nodes.priority,nodes.tid,hwclustercomps.cluster from nodes left join hwclustercomps using (node) where node not in (select node from nodeschedule where rid in (select rid from reservations where state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << end << " and end>" << mysqlpp::quote << begin << " )) order by rand()";
    vector<basenodeinfo> bni;
    query2.storein(bni);
    vector<basenodeinfo>::iterator it2;
    for(it2 = bni.begin(); it2 != bni.end(); ++it2)
    {
      unsigned int parent_label = 0;
      if(!it2->cluster.is_null)
      {
        parent_label = t->get_label(it2->cluster.data);
        if(parent_label == 0) continue;
      }
      onldb_resp r = t->add_node(it2->tid, hwid, parent_label);
      if(r.result() != 1)
      {
        return onldb_resp(-1, (std::string)"database consistency problem");
      }
      ++hwid;
      t->nodes.back()->node = it2->node;
      t->nodes.back()->priority = (int)it2->priority;

      onldb_resp ri = is_infrastructure(it2->node); 
      if(ri.result() < 0) return onldb_resp(-1, (std::string)"database consistency problem");
      if(ri.result() == 1)
      {
        t->nodes.back()->in = t->nodes.back()->label;
      }
    }

    mysqlpp::Query query3 = onl->query();
    query3 << "select cid,capacity,node1,node1port,node2,node2port from connections order by node1,node1port";
    vector<baselinkinfo> bli;
    query3.storein(bli);
    vector<baselinkinfo>::iterator it3;
    for(it3 = bli.begin(); it3 != bli.end(); ++it3)
    {
      if(it3->cid == 0) { continue; }

      int cap = it3->capacity;
      mysqlpp::Query query4 = onl->query();
      query4 << "select capacity from connschedule where cid=" << mysqlpp::quote << it3->cid << " and rid in (select rid from reservations where state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << end << " and end>" << mysqlpp::quote << begin << " )";
      vector<capinfo> ci;
      query4.storein(ci);
      vector<capinfo>::iterator it4;
      for(it4 = ci.begin(); it4 != ci.end(); ++it4)
      {
        cap -= it4->capacity;
      }
      
      if(cap <= 0) { continue; }

      unsigned int node1_label = 0;
      unsigned int node2_label = 0;

      node1_label = t->get_label(it3->node1);
      if(node1_label == 0) { continue; }
      node2_label = t->get_label(it3->node2);
      if(node2_label == 0) { continue; }

      onldb_resp r = t->add_link(linkid, cap, node1_label, it3->node1port, node2_label, it3->node2port);
      if(r.result() != 1) return onldb_resp(-1, (std::string)"database consistency problem");
      t->links.back()->conns.push_back(it3->cid);
 
      bool node1_is_in = false;
      onldb_resp ri = is_infrastructure(it3->node1); 
      if(ri.result() < 0) return onldb_resp(-1, (std::string)"database consistency problem");
      if(ri.result() == 1) { node1_is_in = true; }

      // node2 is always an infrastructure node
      if(!node1_is_in)
      {
        t->links.back()->node1->in = node2_label;
        if(t->links.back()->node1->parent) { t->links.back()->node1->parent->in = node2_label; }
      }

      ++linkid;
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }

  return onldb_resp(1, (std::string)"success");
}

onldb_resp onldb::add_special_node(topology *t, std::string begin, std::string end, node_resource_ptr node) throw()
{
  std::list<node_resource_ptr>::iterator nit;
  std::list<link_resource_ptr>::iterator lit;
  unsigned int hwid = 1;
  unsigned int linkid = 1;
  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    if((*nit)->label > hwid) { hwid = (*nit)->label; }
  }
  ++hwid;
  for(lit = t->links.begin(); lit != t->links.end(); ++lit)
  {
    if((*lit)->label > linkid) { linkid = (*lit)->label; }
  }
  ++linkid;

  try
  {
    mysqlpp::Query query = onl->query();
    query << "select nodes.node,nodes.tid,hwclustercomps.cluster from nodes left join hwclustercomps using (node) where node in (select node from nodeschedule where node=" << mysqlpp::quote << node->node << " and rid in (select rid from reservations where (user='testing' or user='repair' or user='system') and state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << end << " and end>" << mysqlpp::quote << begin << " ))";
    vector<specialnodeinfo> sni;
    query.storein(sni);
    if(sni.empty()) { return onldb_resp(0,(std::string)"node " + node->node + " is not available"); }

    mysqlpp::Query query2 = onl->query();
    query2 << "select distinct node2 from connections where node1=" << mysqlpp::quote << node->node;
    vector<node2info> n2i;
    query2.storein(n2i);
    if(n2i.size() != 1) { return onldb_resp(0,(std::string)"database consistency problem"); }
    unsigned int in = 0;  
    for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
    {
      if((*nit)->node == n2i[0].node2)
      {
        in = (*nit)->in;
        break;
      }
    }
    if(in == 0) { return onldb_resp(0,(std::string)"database consistency problem"); }

    if(sni[0].cluster.is_null)
    {
      onldb_resp anr = t->add_node(sni[0].tid, hwid, 0);
      if(anr.result() != 1) { onldb_resp(0,(std::string)"database consistency problem"); }
      t->nodes.back()->node = node->node;
      t->nodes.back()->fixed = true;
      t->nodes.back()->in = in;
      node->in = in;

      mysqlpp::Query cquery = onl->query();
      cquery << "select cid,capacity,node1,node1port,node2,node2port from connections where node1=" << mysqlpp::quote << node->node;
      vector<baselinkinfo> bli;
      cquery.storein(bli);
      vector<baselinkinfo>::iterator cit;
      for(cit = bli.begin(); cit != bli.end(); ++cit)
      {
        unsigned int node2_label = 0;
        node2_label = t->get_label(cit->node2);
        if(node2_label == 0) { continue; }
        
        onldb_resp alr = t->add_link(linkid, cit->capacity, hwid, cit->node1port, node2_label, cit->node2port);
        if(alr.result() != 1) { onldb_resp(0,(std::string)"database consistency problem"); }
        t->links.back()->conns.push_back(cit->cid);
        linkid++;
      }

      return onldb_resp(1,(std::string)"success");
    }

    mysqlpp::Query pquery = onl->query();
    pquery << "select tid from hwclusters where cluster=" << mysqlpp::quote << sni[0].cluster.data;
    vector<typenameinfo> ps;
    pquery.storein(ps);
    if(ps.size() != 1) { return onldb_resp(0,(std::string)"database consistency problem"); }

    unsigned int parent_label = hwid;
    t->add_node(ps[0].tid, hwid, 0);
    t->nodes.back()->node = sni[0].cluster.data;
    t->nodes.back()->fixed = true;
    t->nodes.back()->in = in;
    ++hwid;

    mysqlpp::Query query3 = onl->query();
    query3 << "select node,tid from nodes where node in (select node from hwclustercomps where cluster=" << mysqlpp::quote << sni[0].cluster << ")";
    vector<specnodeinfo> ni;
    query3.storein(ni);
    vector<specnodeinfo>::iterator niit;
    for(niit = ni.begin(); niit != ni.end(); ++niit)
    {
      onldb_resp anr = t->add_node(niit->tid, hwid, parent_label);
      if(anr.result() != 1) { return onldb_resp(0, (std::string)"database consistency problem"); }
      t->nodes.back()->node = niit->node;
      t->nodes.back()->in = in;
      if(niit->node == node->node)
      {
        t->nodes.back()->fixed = true;
        node->in = in;
        node->parent->fixed = true;
        node->parent->in = in;
        node->parent->node = sni[0].cluster.data;
      }

      mysqlpp::Query cquery = onl->query();
      cquery << "select cid,capacity,node1,node1port,node2,node2port from connections where node1=" << mysqlpp::quote << niit->node;
      vector<baselinkinfo> bli;
      cquery.storein(bli);
      vector<baselinkinfo>::iterator cit;
      for(cit = bli.begin(); cit != bli.end(); ++cit)
      {
        unsigned int node2_label = 0;
        node2_label = t->get_label(cit->node2);
        if(node2_label == 0) { continue; }

        onldb_resp alr = t->add_link(linkid, cit->capacity, hwid, cit->node1port, node2_label, cit->node2port);
        if(alr.result() != 1) { onldb_resp(0,(std::string)"database consistency problem"); }
        t->links.back()->conns.push_back(cit->cid);
        linkid++;
      }
      ++hwid;
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }

  return onldb_resp(1, (std::string)"success");
}

//JP changed to set remapped reservation to "used" 3/29/2012
//onldb_resp onldb::try_reservation(topology *t, std::string user, std::string begin, std::string end) throw()
onldb_resp onldb::try_reservation(topology *t, std::string user, std::string begin, std::string end, std::string state) throw()
{
  //first create a list of the nodes separated by type
  std::list<assign_info_ptr> assign_list;

  std::list<assign_info_ptr>::iterator ait;
  std::list<node_resource_ptr>::iterator nit;
  std::list<node_resource_ptr>::iterator fixed_comp;
  std::list<link_resource_ptr>::iterator lit;

  std::list<node_resource_ptr> fixed_comps;

  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    (*nit)->marked = false;
    if((*nit)->is_parent) { continue; }

    if((*nit)->fixed)
    {
      fixed_comps.push_back(*nit);
    }

    for(ait = assign_list.begin(); ait != assign_list.end(); ++ait)
    {
      if((*ait)->type == (*nit)->type)
      {
        (*ait)->user_nodes.push_back(*nit);
        break;
      }
    }
    if(ait == assign_list.end())
    {
      assign_info_ptr newnode(new assign_info());
      newnode->type = (*nit)->type;
      std::string tt = get_type_type((*nit)->type);
      if(tt == "") return onldb_resp(-1, (std::string)"database consistency problem");
      newnode->type_type = tt;
      newnode->user_nodes.push_back(*nit);
      assign_list.push_back(newnode);
    }
  }

  // sort the requested list by increasing number of comps to facilitate matching
  assign_list.sort(req_sort_comp);

  // next build the base topology
  topology base_top;
  onldb_resp r = get_base_topology(&base_top, begin, end);
  if(r.result() < 1) return onldb_resp(r.result(),r.msg());

  onldb_resp ra = is_admin(user);
  if(ra.result() < 0) return onldb_resp(ra.result(),ra.msg());
  bool admin = false;
  if(ra.result() == 1) { admin = true; }

  // handle fixed components, potentially adding admin stuff to the base topology
  for(fixed_comp = fixed_comps.begin(); fixed_comp != fixed_comps.end(); ++fixed_comp)
  {
    for(nit = base_top.nodes.begin(); nit != base_top.nodes.end(); ++nit)
    {
      if((*nit)->node == (*fixed_comp)->node)
      {
        (*fixed_comp)->in = (*nit)->in;
        (*nit)->fixed = true;
        if((*nit)->parent)
        {
          (*fixed_comp)->parent->fixed = true;
          (*fixed_comp)->parent->in = (*nit)->in;
          (*nit)->parent->fixed = true;
          (*fixed_comp)->parent->node = (*nit)->parent->node;
        }
        break;
      }
    }
    if(nit == base_top.nodes.end())
    {
      if(!admin)
      {
        return onldb_resp(0, "fixed node " + (*fixed_comp)->node + " is not available");
      }
      onldb_resp fcr = add_special_node(&base_top, begin, end, *fixed_comp);
      if(fcr.result() < 1) return onldb_resp(fcr.result(), fcr.msg());
    }
  }

  // add everything from the base topology to the assign list
  for(nit = base_top.nodes.begin(); nit != base_top.nodes.end(); ++nit)
  {
    (*nit)->marked = false;
    if((*nit)->is_parent) { continue; }

    for(ait = assign_list.begin(); ait != assign_list.end(); ++ait)
    {
      if((*ait)->type == (*nit)->type)
      {
        (*nit)->type_type = (*ait)->type_type;
        (*ait)->testbed_nodes.push_back(*nit);
        break;
      }
    }
    if(ait == assign_list.end())
    {
      assign_info_ptr newnode(new assign_info());
      newnode->type = (*nit)->type;
      std::string tt = get_type_type((*nit)->type);
      if(tt == "") return onldb_resp(-1, (std::string)"database consistency problem");
      newnode->type_type = tt;
      (*nit)->type_type = tt;
      newnode->testbed_nodes.push_back(*nit);
      assign_list.push_back(newnode);
    }
  }
  
  // check that the the number of each requested type is <= to the number available
  for(ait = assign_list.begin(); ait != assign_list.end(); ++ait)
  {
    if((*ait)->type == "vgige") { continue; }
    if((*ait)->user_nodes.size() > (*ait)->testbed_nodes.size())
    {
      std::string s = "too many " + (*ait)->type + "s already reserved";
      return onldb_resp(0,s);
    }
    
    // sort the base comp list by priority
    (*ait)->testbed_nodes.sort(base_sort_comp);
  }
      
  for(nit = base_top.nodes.begin(); nit != base_top.nodes.end(); ++nit)
  {
    (*nit)->marked = false;
    (*nit)->mip_id = 0;
  }
  for(lit = base_top.links.begin(); lit != base_top.links.end(); ++lit)
  {
    (*lit)->marked = false;
  }
  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    if(!((*nit)->fixed))
    {
      (*nit)->node = "";
    }
    (*nit)->marked = false;
    (*nit)->mip_id = 0;
  }
  for(lit = t->links.begin(); lit != t->links.end(); ++lit)
  {
    (*lit)->marked = false;
    (*lit)->conns.clear();
  }

  try
  {
    if(find_embedding(t, &base_top, assign_list))
    {
      onldb_resp r = add_reservation(t,user,begin,end,state);//JP changed 3/29/2012
      if(r.result() < 1) return onldb_resp(r.result(),r.msg());
      std::string s = "success! reservation made from " + begin + " to " + end;
      return onldb_resp(1,s);
    }
  }
  catch(GRBException& e)
  {
    cerr << "Error code = " << e.getErrorCode() << endl;
    cerr << e.getMessage() << endl;
    return onldb_resp(0,(std::string)"solver error");
  }

  return onldb_resp(0,(std::string)"topology doesn't fit during that time");
}

bool onldb::find_embedding(topology* req, topology* base, std::list<assign_info_ptr> al) throw(GRBException)
{
  std::list<node_resource_ptr>::iterator basenit;
  std::list<node_resource_ptr>::iterator reqnit;
  std::list<link_resource_ptr>::iterator baselit;
  std::list<link_resource_ptr>::iterator reqlit;
  std::list<assign_info_ptr>::iterator ait;

  GRBEnv grbenv = GRBEnv();
  grbenv.set(GRB_IntParam_OutputFlag, 0);
  GRBModel mip = GRBModel(grbenv);

  int num_nodes_base = 0;
  int num_in_base = 0;
  int num_in_aug_base = 0;
  int num_in_aug_only_base = 0;

  int num_nodes_req = req->nodes.size();
  int num_commodities = req->links.size();

  int num_types_vsw = 0;

  std::map<int,int> in2mip;
  std::map<int,int> mip2in;

  // calculate all needed parameters from testbed and user graphs
  int base_node_id = 1;
  int req_node_id = 1;
  for(ait = al.begin(); ait != al.end(); ++ait)
  {
    if((*ait)->type_type == "infrastructure")
    {
      num_nodes_base += (*ait)->testbed_nodes.size();
      num_in_base += (*ait)->testbed_nodes.size();
      num_in_aug_base += (*ait)->testbed_nodes.size();

      for(basenit = (*ait)->testbed_nodes.begin(); basenit != (*ait)->testbed_nodes.end(); ++basenit)
      {
        (*basenit)->mip_id = base_node_id;
        in2mip[(*basenit)->in] = base_node_id;
        mip2in[base_node_id] = (*basenit)->in;
        ++base_node_id;
      }
    }

    if((*ait)->user_nodes.empty()) continue;
    ++num_types_vsw;

    for(reqnit = (*ait)->user_nodes.begin(); reqnit != (*ait)->user_nodes.end(); ++reqnit)
    {
      (*reqnit)->mip_id = req_node_id;
      ++req_node_id;
    }
  }

  num_nodes_base += num_types_vsw;
  for(baselit = base->links.begin(); baselit != base->links.end(); ++baselit)
  {
    if((*baselit)->node1->type_type == "infrastructure" && (*baselit)->node2->type_type == "infrastructure")
    {
      ++num_nodes_base;
      ++num_in_aug_base;
      ++num_in_aug_only_base;
    }
  }
  // done calculating stuff directly from graphs
  
  // calculate capacities and costs for all edges
  int cap[num_nodes_base+1][num_nodes_base+1];
  double cost[num_in_aug_base+1][num_in_aug_base+1];
  for(int u=1; u<=num_nodes_base; ++u)
  {
    for(int v=1; v<=num_nodes_base; ++v)
    {
      cap[u][v] = 0;
    }
  }
  for(int u=1; u<=num_in_aug_base; ++u)
  {
    for(int v=1; v<=num_in_aug_base; ++v)
    {
      cost[u][v] = 0;
    }
  }
  std::map<int,link_resource_ptr> multigraph_nodes;
  int multigraph_node = num_in_base + 1;
  int lastu = 0;
  int lastv = 0;
  double nextcost = 1.0;
  for(baselit = base->links.begin(); baselit != base->links.end(); ++baselit)
  {
    if((*baselit)->node1->type_type == "infrastructure" && (*baselit)->node2->type_type == "infrastructure")
    {
      int u = (*baselit)->node1->mip_id;
      int v = (*baselit)->node2->mip_id;

      int lcap = (*baselit)->capacity;
      cap[u][multigraph_node] = lcap;
      cap[v][multigraph_node] = lcap;
      cap[multigraph_node][u] = lcap;
      cap[multigraph_node][v] = lcap;

      if(lastu == u && lastv == v) { nextcost += 0.01; }
      else { nextcost = 1.0; }
      lastu = u;
      lastv = v;
      cost[u][multigraph_node] = nextcost;
      cost[v][multigraph_node] = nextcost;
      cost[multigraph_node][u] = nextcost;
      cost[multigraph_node][v] = nextcost;

      multigraph_nodes[multigraph_node] = *baselit;
      ++multigraph_node;
    }
  }
  for(int u=num_in_aug_base+1; u<=num_nodes_base; ++u)
  {
    for(int v=1; v<=num_in_base; ++v)
    {
      cap[u][v] = 10000;
      cap[v][u] = 10000;
    }
  }
  // done setting capacities and costs

  // variables

  // flow
  GRBVar flow[num_commodities+1][num_nodes_base+1][num_nodes_base+1];
  for(int c=1; c<=num_commodities; ++c)
  {
    for(int v1=1; v1<=num_nodes_base; ++v1)
    {
      for(int v2=1; v2<=num_nodes_base; ++v2)
      {
        if(v1 <= num_in_aug_base && v2 <= num_in_aug_base)
        {
          flow[c][v1][v2] = mip.addVar(0.0, GRB_INFINITY, cost[v1][v2], GRB_CONTINUOUS);     
        }
        else
        {
          flow[c][v1][v2] = mip.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS);
        }
      }
    }
  }

  // flow to edge assignments
  GRBVar finf[num_commodities+1][num_in_base+1][num_in_aug_only_base+1];
  for(int c=1; c<=num_commodities; ++c)
  {
    for(int in=1; in<=num_nodes_base; ++in)
    {
      for(int ee=num_in_base+1; ee<=num_in_aug_base; ++ee)
      {
        finf[c][in][ee-num_in_base] = mip.addVar(0.0, 1.0, 0.0, GRB_BINARY);
      }
    }
  }

  // node to switch assignments
  GRBVar inf[num_in_base+1][num_nodes_req+1];
  for(int in=1; in<=num_in_base; ++in)
  {
    for(int u=1; u<=num_nodes_req; ++u)
    {
      inf[in][u] = mip.addVar(0.0, 1.0, 0.0, GRB_BINARY);
    }
  }

  // integrate variables into model
  mip.update();

  // constraints

  // capacity constraints
  for(int v1=1; v1<=num_nodes_base; ++v1)
  {
    for(int v2=1; v2<=num_nodes_base; ++v2)
    {
      GRBLinExpr con;
      for(int c=1; c<=num_commodities; ++c)
      {
        con += flow[c][v1][v2];
      }
      mip.addConstr(con <= cap[v1][v2]);
    }
  }

  // additional capacity constraints
  for(int v1=1; v1<=num_in_aug_base; ++v1)
  {
    for(int v2=1; v2<=num_in_aug_base; ++v2)
    {
      GRBLinExpr con;
      for(int c=1; c<=num_commodities; ++c)
      {
        con += flow[c][v1][v2];
        con += flow[c][v2][v1];
      }
      mip.addConstr(con <= cap[v1][v2]);
    }
  }

  // each user node is mapped to one testbed node
  for(int u=1; u<=num_nodes_req; ++u)
  {
    GRBLinExpr con;
    for(int in=1; in<=num_in_base; ++in)
    {
      con += inf[in][u];
    }
    mip.addConstr(con == 1);
  }

  // limit number of nodes of each type assigned to each switch to whats available
  std::map<std::string, int> type_to_node;
  int next_type_node = num_in_aug_base + 1;
  for(ait = al.begin(); ait != al.end(); ++ait)
  {
    if((*ait)->user_nodes.empty()) continue;
    type_to_node[(*ait)->type] = next_type_node;
    ++next_type_node;
    if((*ait)->type == "vgige") continue;

    int num_per_in[num_in_base+1];
    for(int in=1; in<=num_in_base; ++in) { num_per_in[in] = 0; }
    for(basenit = (*ait)->testbed_nodes.begin(); basenit != (*ait)->testbed_nodes.end(); ++basenit)
    {
      int mip_id = in2mip[(*basenit)->in];
      num_per_in[mip_id] += 1;
    }

    for(int in=1; in<=num_in_base; ++in)
    {
      GRBLinExpr con;
      for(reqnit = (*ait)->user_nodes.begin(); reqnit != (*ait)->user_nodes.end(); ++reqnit)
      {
        con += inf[in][(*reqnit)->mip_id];
      }
      mip.addConstr(con <= num_per_in[in]);
    }
  }

  // per commodity flows between infrastructure nodes are all or nothing
  for(int c=1; c<=num_commodities; ++c)
  {
    for(int in=1; in<=num_in_base; ++in)
    {
      GRBLinExpr con;
      for(int ee=num_in_base+1; ee<=num_in_aug_base; ++ee)
      {
        con += finf[c][in][ee-num_in_base];
      }
      mip.addConstr(con <= 1);
    }
  }

  // per commodity flow assignments
  int c;
  for(c=1, reqlit = req->links.begin(); reqlit != req->links.end(); ++reqlit, ++c)
  {
    for(int in=1; in<=num_in_base; ++in)
    {
      for(int ee=num_in_base+1; ee<=num_in_aug_base; ++ee)
      {
        mip.addConstr(flow[c][in][ee] == ((*reqlit)->capacity)*finf[c][in][ee-num_in_base]);
      }
    }
  }

  // per commodity flow conservation
  for(c=1; c<=num_commodities; ++c)
  {
    for(int v1=1; v1<=num_in_aug_base; ++v1)
    {
      GRBLinExpr con;
      for(int v2=1; v2<=num_nodes_base; ++v2)
      {
        con += flow[c][v1][v2];
        con += -1*flow[c][v2][v1];
      }
      mip.addConstr(con == 0);
    }
  }

  // per commodity source/sink assignments
  for(c=1, reqlit = req->links.begin(); reqlit != req->links.end(); ++reqlit, ++c)
  {
    int hw1_tn = type_to_node[(*reqlit)->node1->type];
    int hw2_tn = type_to_node[(*reqlit)->node2->type];
    for(int in=1; in<=num_in_base; ++in)
    {
      for(ait = al.begin(); ait != al.end(); ++ait)
      {
        if((*ait)->user_nodes.empty()) continue;
        int this_tn = type_to_node[(*ait)->type];

        GRBLinExpr rhs1;
        if(hw1_tn == this_tn)
        {
          rhs1 += ((*reqlit)->capacity)*inf[in][(*reqlit)->node1->mip_id];
        }
        else
        {
          rhs1 += 0;
        }
        mip.addConstr(flow[c][this_tn][in] == rhs1);

        GRBLinExpr rhs2;
        if(hw2_tn == this_tn)
        {
          rhs2 += ((*reqlit)->capacity)*inf[in][(*reqlit)->node2->mip_id];
        }
        else
        {
          rhs2 += 0;
        }
        mip.addConstr(flow[c][in][this_tn] == rhs2);
      }
    }
  }

  // clusters stay on the same switch
  for(reqnit = req->nodes.begin(); reqnit != req->nodes.end(); ++reqnit)
  {
    if((*reqnit)->type_type != "hwcluster") { continue; }
    int node1_id = (*reqnit)->node_children.front()->mip_id;
    for(int in=1; in<=num_in_base; ++in) 
    {
      std::list<node_resource_ptr>::iterator cit;
      cit = (*reqnit)->node_children.begin();
      ++cit;
      for(; cit != (*reqnit)->node_children.end(); ++cit)
      {
        int node_id = (*cit)->mip_id;
        mip.addConstr(inf[in][node1_id] == inf[in][node_id]);
      }
    }
  }

  // everything on a vswitch stays on the same switch
  for(ait = al.begin(); ait != al.end(); ++ait)
  {
    if((*ait)->type != "vgige") continue;
    for(reqnit = (*ait)->user_nodes.begin(); reqnit != (*ait)->user_nodes.end(); ++reqnit)
    {
      int vsw_node = (*reqnit)->mip_id;
      for(int in=1; in<=num_in_base; ++in)
      {
        for(reqlit = (*reqnit)->links.begin(); reqlit != (*reqnit)->links.end(); ++reqlit)
        {
          if((*reqlit)->node1->type == "vgige" && ((*reqlit)->node2->type == "vgige")) { continue; }
          int node = (*reqlit)->node1->mip_id;
          if((*reqlit)->node1->type == "vgige") { node = (*reqlit)->node2->mip_id; }
  
          mip.addConstr(inf[in][vsw_node] == inf[in][node]);
        }
      }
    }
    break;
  }   

  // deal with fixed nodes
  for(reqnit = req->nodes.begin(); reqnit != req->nodes.end(); ++reqnit)
  {
    if((*reqnit)->is_parent) continue;
    if(!((*reqnit)->fixed)) continue;
    int node = (*reqnit)->mip_id;
    int in_mip = in2mip[(*reqnit)->in];

    mip.addConstr(inf[in_mip][node] == 1);
  }

  // solve the mip
  mip.update();
  mip.optimize();
  int stat = mip.get(GRB_IntAttr_Status);
  if(stat != GRB_OPTIMAL && stat != GRB_SUBOPTIMAL) { return false; }

  for(reqnit = req->nodes.begin(); reqnit != req->nodes.end(); ++reqnit)
  {
    if((*reqnit)->is_parent) { continue; }

    int node = (*reqnit)->mip_id;
    int in;
    for(in=1; in<=num_in_base; ++in)
    {
      double infd = inf[in][node].get(GRB_DoubleAttr_X);
      int infi = (int)(infd+0.5);
      if(infi == 1)
      {
        (*reqnit)->in = mip2in[in];
        if((*reqnit)->parent) { (*reqnit)->parent->in = (*reqnit)->in; }
        break;
      }
    }
    if(in == num_in_base+1) { return false; }
  }

  for(c=1, reqlit = req->links.begin(); reqlit != req->links.end(); ++reqlit, ++c)
  {
    int src_in = in2mip[(*reqlit)->node1->in];
    int snk_in = in2mip[(*reqlit)->node2->in];
    int nxt_in = src_in;
    int lst_in = src_in;
    while(nxt_in != snk_in)
    {
      int v;
      for(v=1; v<=num_in_aug_base; ++v)
      {
        if(v == lst_in) { continue; }
        double f = flow[c][nxt_in][v].get(GRB_DoubleAttr_X);
        int fi = (int)(f+0.5);
        if(fi == 0) { continue; }
        if(fi != (int)(*reqlit)->capacity) { return false; }
        break;
      }
      if(v == num_in_aug_base+1) { return false; }

      link_resource_ptr lnk = multigraph_nodes[v];
      (*reqlit)->conns.push_back(lnk->conns.front());

      if(lnk->node1->mip_id == (unsigned int)nxt_in)
      {
        nxt_in = lnk->node2->mip_id;
      }
      else
      {
        nxt_in = lnk->node1->mip_id;
      }
      lst_in = v;
    }
  }

  // embed fixed nodes and clusters first
  for(reqnit = req->nodes.begin(); reqnit != req->nodes.end(); ++reqnit)
  {
    if((*reqnit)->is_parent)
    {
      for(basenit = base->nodes.begin(); basenit != base->nodes.end(); ++basenit)
      {
        if((*basenit)->marked) { continue; }
        if((*basenit)->type != (*reqnit)->type) { continue; }
        if((*basenit)->in != (*reqnit)->in) { continue; }
        
        if(((*reqnit)->fixed || (*basenit)->fixed) && ((*reqnit)->node != (*basenit)->node)) { continue; }
        
        // here, we've found  a cluster parent not yet used, of the right type, on the 
        // right in.  try to map each child node in the req to one in the testbed
        std::list<node_resource_ptr>::iterator bcit;
        std::list<node_resource_ptr>::iterator rcit;
        for(rcit = (*reqnit)->node_children.begin(); rcit != (*reqnit)->node_children.end(); ++rcit)
        {
          for(bcit = (*basenit)->node_children.begin(); bcit != (*basenit)->node_children.end(); ++bcit)
          {
            if((*bcit)->marked) { continue; }
            if((*bcit)->type != (*rcit)->type) { continue; }
            if(((*rcit)->fixed || (*bcit)->fixed) && ((*rcit)->node != (*bcit)->node)) { continue; }
            if(embed(*rcit, *bcit) == false) return false;
            break;
          }
          if(bcit == (*basenit)->node_children.end()) { return false; }
        }
         
        (*reqnit)->node = (*basenit)->node;
        (*basenit)->marked = true;
        break;
      }
      if(basenit == base->nodes.end()) return false;
    }
    else if((*reqnit)->fixed && !((*reqnit)->parent))
    {
      for(basenit = base->nodes.begin(); basenit != base->nodes.end(); ++basenit)
      {
        if((*reqnit)->node == (*basenit)->node)
        {
          if(embed(*reqnit, *basenit) == false) return false;
          break;
        }
      }
      if(basenit == base->nodes.end()) return false;
    }
  }

  for(ait = al.begin(); ait != al.end(); ++ait)
  {
    if((*ait)->user_nodes.empty()) continue;
    if((*ait)->type == "vgige") continue;

    for(reqnit = (*ait)->user_nodes.begin(); reqnit != (*ait)->user_nodes.end(); ++reqnit)
    {
      if((*reqnit)->marked) continue;
      basenit = (*ait)->testbed_nodes.begin();
      while(basenit != (*ait)->testbed_nodes.end() && (((*basenit)->marked) || ((*basenit)->in != (*reqnit)->in))) ++basenit;
      if(basenit == (*ait)->testbed_nodes.end()) return false;
      if(embed(*reqnit, *basenit) == false) return false;
    }
  }

  return true;
}

bool onldb::embed(node_resource_ptr user, node_resource_ptr testbed) throw()
{
  std::list<link_resource_ptr>::iterator tlit;
  std::list<link_resource_ptr>::iterator ulit;

  if(user->marked || testbed->marked) return false;
 
  if(user->fixed)
  {
    if(user->node != testbed->node) return false;
  }

  user->node = testbed->node;
  user->marked = true;
  testbed->marked = true;
  
  for(ulit = user->links.begin(); ulit != user->links.end(); ++ulit)
  {
    bool loopback = false;
    if((*ulit)->node1->label == (*ulit)->node2->label) { loopback = true; }

    bool user_this_is_node1 = false;
    unsigned int user_this_port = (*ulit)->node2_port;
    if(loopback)
    {
      if((*ulit)->conns.empty())
      {
        user_this_is_node1 = true;
        user_this_port = (*ulit)->node1_port;
      }
    }
    else if((*ulit)->node1->label == user->label)
    {
      user_this_is_node1 = true;
      user_this_port = (*ulit)->node1_port;
    }

    for(tlit = testbed->links.begin(); tlit != testbed->links.end(); ++tlit)
    {
      unsigned int testbed_port = (*tlit)->node2_port;
      if((*tlit)->node1->label == testbed->label)
      {
        testbed_port = (*tlit)->node1_port;
      }
      if(testbed_port == user_this_port) break;
    }
    if(tlit == testbed->links.end()) return false;

    if(user_this_is_node1)
    {
      (*ulit)->conns.push_front((*tlit)->conns.front());
    }
    else
    {
      (*ulit)->conns.push_back((*tlit)->conns.front());
    }
  }

  return true;
}

//JP change for remap so reservation set as used 3/29/2012
//onldb_resp onldb::add_reservation(topology *t, std::string user, std::string begin, std::string end) throw()
onldb_resp onldb::add_reservation(topology *t, std::string user, std::string begin, std::string end, std::string state) throw()
{
  try
  {
    // add the reservation entry
    reservationins res(user, mysqlpp::DateTime(begin), mysqlpp::DateTime(end), state);//"pending");//JP change 3/29/2012
    mysqlpp::Query ins = onl->query();
    ins.insert(res);
    mysqlpp::SimpleResult sr = ins.execute();

    unsigned int rid = sr.insert_id();

    std::list<node_resource_ptr>::iterator nit;
    std::list<link_resource_ptr>::iterator lit;
    std::list<int>::iterator cit;

    unsigned int vlanid = 1;
    unsigned int linkid = 1;

    // need to give unique names to every vswitch first
    for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
    {
      if((*nit)->type == "vgige")
      {
        (*nit)->node = "vgige" + to_string(vlanid);
        (*nit)->mip_id = vlanid;
        ++vlanid;
      }
    }

    for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
    {
      unsigned int fixed = 0;
      if((*nit)->fixed == true)
      {
        fixed = 1;
      }

      if((*nit)->type == "vgige")
      {
        // add the virtual switch entries
        for(lit = (*nit)->links.begin(); lit != (*nit)->links.end(); ++lit)
        {
          if((*lit)->linkid == 0)
          {
            if((*lit)->conns.empty())
            {
              // if a vswitch->vswitch connection, and both are mapped to same switch,
              // then there are no cids in the list. as a hack to make this work, there
              // is a cid=0 entry in the table that we use here so that the necessary
              // table entries are there for every link
              (*lit)->conns.push_back(0);
            }
            for(cit = (*lit)->conns.begin(); cit != (*lit)->conns.end(); ++cit)
            {
              connschedule cs(linkid, rid, *cit, (*lit)->capacity);
              mysqlpp::Query ins = onl->query();
              ins.insert(cs);
              ins.execute();
            }
            (*lit)->linkid = linkid;
            ++linkid;
          }

          int port = (*lit)->node1_port;
          if((*lit)->node2->node == (*nit)->node) 
          {
            port = (*lit)->node2_port;
          }

          vswitchschedule vs((*nit)->mip_id, rid, port, (*lit)->linkid);
          mysqlpp::Query vins = onl->query();
          vins.insert(vs);
          vins.execute();
        }
      }
      else if((*nit)->type_type == "hwcluster")
      {
        // add the hwcluster entries
        hwclusterschedule hwcs((*nit)->node, rid, fixed);
        mysqlpp::Query ins = onl->query();
        ins.insert(hwcs);
        ins.execute();
      }
      else
      {
        // add the node entries
        nodeschedule ns((*nit)->node, rid, fixed);
        mysqlpp::Query ins = onl->query();
        ins.insert(ns);
        ins.execute();
      }
    }

    for(lit = t->links.begin(); lit != t->links.end(); ++lit)
    {
      if((*lit)->node1->type == "vgige" || (*lit)->node2->type == "vgige") continue;
      // add the link entries
      for(cit = (*lit)->conns.begin(); cit != (*lit)->conns.end(); ++cit)
      {
        connschedule cs(linkid, rid, *cit, (*lit)->capacity);
        mysqlpp::Query ins = onl->query();
        ins.insert(cs);
        ins.execute();
      }
      ++linkid;
    }
  }
  catch (const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::check_interswitch_bandwidth(topology* t, std::string begin, std::string end) throw()
{
  std::list<link_resource_ptr>::iterator lit;
  std::list<int>::iterator cit;

  std::map<int, int> caps;
  std::map<int, int>::iterator mit;

  for(lit = t->links.begin(); lit != t->links.end(); ++lit)
  {
    for(cit = (*lit)->conns.begin(); cit != (*lit)->conns.end(); ++cit) 
    {
      if(*cit == 0) { continue; }
      if(caps.find(*cit) == caps.end())
      {
        caps[*cit] = (*lit)->capacity;
      }
      else
      {
        caps[*cit] += (*lit)->capacity;
      }
    }
  }
  
  try
  {
    for(mit = caps.begin(); mit != caps.end(); ++mit)
    {
      mysqlpp::Query query = onl->query();
      query << "select capacity from connschedule where cid=" << mysqlpp::quote << mit->first << " and rid in (select rid from reservations where state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << end << " and end>" << mysqlpp::quote << begin << ")";
      vector<capinfo> ci;
      query.storein(ci);
      vector<capinfo>::iterator cap;
      for(cap = ci.begin(); cap != ci.end(); ++cap) 
      {
        caps[mit->first] += cap->capacity;
      }

      mysqlpp::Query query2 = onl->query();
      query2 << "select capacity from connections where cid=" << mysqlpp::quote << mit->first;
      vector<capinfo> ci2;
      query2.storein(ci2);
      if(ci2.size() != 1) { return onldb_resp(-1, (std::string)"database consistency problem"); }
      if(caps[mit->first] > ci2[0].capacity)
      {
        return onldb_resp(0,(std::string)"too many resources in use");
      }
    }
  }
  catch (const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::print_types() throw()
{
  try
  {
    vector<types> typesres;
    mysqlpp::Query query = onl->query();

    query << "select * from types";
    query.storein(typesres);

    vector<types>::iterator type;
    for(type = typesres.begin(); type != typesres.end(); ++type)
    {
      cout << type->tid << endl;
    }
  }
  catch (const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::clear_all_experiments() throw()
{
  try
  {
    // first get the list of active experiments so that we can clear out 
    // all soft state (groups, exports) that may be left over from before
    mysqlpp::Query oldres = onl->query();
    oldres << "select rid,user from reservations where rid in (select rid from experiments where begin=end)";
    vector<userres> reses;
    oldres.storein(reses);
    vector<userres>::iterator res;
    for(res = reses.begin(); res != reses.end(); ++res)
    {
      mysqlpp::Query nodeq = onl->query();
      nodeq << "select node,daemonhost,acl from nodes where node in (select node from nodeschedule where rid=" << mysqlpp::quote << res->rid << ")"; 
      vector<oldnodes> nodes;
      nodeq.storein(nodes);
      vector<oldnodes>::iterator node;
      for(node = nodes.begin(); node != nodes.end(); ++node)
      {
        std::string cmd = "/usr/testbed/scripts/system_session_update.pl remove " + res->user + " " + node->daemonhost + " " + node->acl;
        int ret = system(cmd.c_str());
        if(ret < 0 || WEXITSTATUS(ret) != 1) cout << "Warning: " << res->user << "'s home area was not unexported to " << node->daemonhost << " and user was not removed from group " << node->acl << endl;
      }

      mysqlpp::Query clusq = onl->query();
      clusq << "select cluster,acl from hwclusters where cluster in (select cluster from hwclusterschedule where rid=" << mysqlpp::quote << res->rid << ")"; 
      vector<oldclusters> clusters;
      clusq.storein(clusters);
      vector<oldclusters>::iterator cluster;
      for(cluster = clusters.begin(); cluster != clusters.end(); ++cluster)
      {
        std::string cmd = "/usr/testbed/scripts/system_session_update.pl remove " + res->user + " unused " + cluster->acl;
        int ret = system(cmd.c_str());
        if(ret < 0 || WEXITSTATUS(ret) != 1) cout << "Warning: " << res->user << " was not removed from group " << cluster->acl << endl;
      }
    }
    if(!reses.empty())
    {
      std::string cmd = "/usr/testbed/scripts/system_session_update.pl update";
      int ret = system(cmd.c_str());
      if(ret < 0 || WEXITSTATUS(ret) != 1) cout << "clear_all_experiments: warning: update failed, so groups may not have been cleaned up" << endl;
    }

    // update any 'active' reservations to be used
    mysqlpp::Query resup = onl->query();
    resup << "update reservations set state='used' where state='active'";
    resup.execute();
  
    // update any 'active' experiments to have now as their end time 
    std::string current_time = time_unix2db(time(NULL));
    mysqlpp::Query expup = onl->query();
    expup << "update experiments set end=" << mysqlpp::quote << current_time << " where begin=end";
    expup.execute();

    // update all node states that are not testing,spare,repair to be free
    mysqlpp::Query nodeup = onl->query();
    nodeup << "update nodes set state='free' where state='active' or state='initializing' or state='refreshing'";
    nodeup.execute();

    // update all hwcluster states that are not testing,spare,repair to be free
    mysqlpp::Query clup = onl->query();
    clup << "update hwclusters set state='free' where state='active' or state='initializing' or state='refreshing'";
    clup.execute();
  }
  catch (const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::get_switch_list(switch_info_list& list) throw()
{
  list.clear();
  try
  {
    vector<nodenames> switches;
    mysqlpp::Query query = onl->query();
    query << "select node from nodes where tid in (select tid from types where type='infrastructure')";
    query.storein(switches);
    vector<nodenames>::iterator sw;
    for(sw = switches.begin(); sw != switches.end(); ++sw)
    {
      mysqlpp::Query query2 = onl->query();
      query2 << "select max(port) as numports from interfaces where tid in (select tid from nodes where node=" << mysqlpp::quote << sw->node << ")";
      mysqlpp::StoreQueryResult res2 = query2.store();
      if(res2.empty())
      {
        return onldb_resp(-1,"database inconsistency error");
      }
      switchports sp = res2[0];

      mysqlpp::Query query3 = onl->query();
      query3 << "select port from interfaces where interface='management' and tid in (select tid from nodes where node=" << mysqlpp::quote << sw->node << ")";
      mysqlpp::StoreQueryResult res3 = query3.store();
      unsigned int mgmt_port = 0;
      if(!res3.empty())
      {
        mgmtport mp = res3[0];
        mgmt_port = mp.port;
      }

      switch_info si(sw->node, sp.numports, mgmt_port);
      list.push_back(si);
    }
  }
  catch (const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::get_base_node_list(node_info_list& list) throw()
{
  list.clear();
  try
  {
    vector<nodeinfo> nodes;
    mysqlpp::Query query = onl->query();
    query << "select nodes.node,nodes.state,types.daemon,types.keeboot,nodes.daemonhost,nodes.daemonport,types.tid as type,hwclustercomps.dependent from nodes join types using (tid) left join hwclustercomps using (node) where types.type='base' order by node";
    query.storein(nodes);

    vector<nodeinfo>::iterator node;
    for(node = nodes.begin(); node != nodes.end(); ++node)
    {
      bool has_cp;
      if(node->daemon == 0) { has_cp = false; }
      else { has_cp = true; }
      bool do_keeboot;
      if(node->keeboot == 0) { do_keeboot = false; }
      else { do_keeboot = true; }
      bool is_dependent;
      if(node->dependent.is_null) { is_dependent = false; }
      else if(node->dependent.data == 0) { is_dependent = false; }
      else { is_dependent = true; }
      node_info new_node(node->node,node->state,has_cp,do_keeboot,node->daemonhost,node->daemonport,node->type,is_dependent);
      list.push_back(new_node);
    }
  }
  catch (const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::get_node_info(std::string node, bool is_cluster, node_info& info) throw()
{
  if(node.substr(0,5) == "vgige")
  {
    info = node_info(node,"free",false,false,"",0,"vgige",false);
    return onldb_resp(1,(std::string)"success");
  }
  try
  {
    if(!is_cluster)
    {
      mysqlpp::Query query = onl->query();
      query << "select nodes.node,nodes.state,types.daemon,types.keeboot,nodes.daemonhost,nodes.daemonport,types.tid as type,hwclustercomps.dependent from nodes join types using (tid) left join hwclustercomps using (node) where types.type='base' and nodes.node=" << mysqlpp::quote << node;
      mysqlpp::StoreQueryResult res = query.store();

      if(res.empty())
      {
        std::string errmsg;
        errmsg = "node " + node + " not in the database";
        return onldb_resp(-1,errmsg);
      }
      nodeinfo ni = res[0];
    
      bool has_cp;
      if(ni.daemon == 0) { has_cp = false; }
      else { has_cp = true; }
      bool do_keeboot;
      if(ni.keeboot == 0) { do_keeboot = false; }
      else { do_keeboot = true; }
      bool is_dependent;
      if(ni.dependent.is_null) { is_dependent = false; }
      else if(ni.dependent.data == 0) { is_dependent = false; }
      else { is_dependent = true; }
      info = node_info(ni.node,ni.state,has_cp,do_keeboot,ni.daemonhost,ni.daemonport,ni.type,is_dependent);
    }
    else
    {
      mysqlpp::Query query = onl->query();
      query << "select hwclusters.cluster as cluster,hwclusters.state as state,types.tid as type from hwclusters,types where types.type='hwcluster' and hwclusters.tid=types.tid and hwclusters.cluster=" << mysqlpp::quote << node;
      mysqlpp::StoreQueryResult res = query.store();

      if(res.empty())
      {
        std::string errmsg;
        errmsg = "node " + node + " not in the database";
        return onldb_resp(-1,errmsg);
      }
      clusterinfo ci = res[0];
    
      info = node_info(ci.cluster,ci.state,false,false,"",0,ci.type,false);
    }
  }
  catch (const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::get_state(std::string node, bool is_cluster) throw()
{
  if(node.substr(0,5) == "vgige")
  {
    return onldb_resp(1,(std::string)"free");
  }
  try
  {
    if(!is_cluster)
    {
      mysqlpp::Query query = onl->query();
      query << "select state from nodes where node=" << mysqlpp::quote << node;
      mysqlpp::StoreQueryResult res = query.store();
      if(res.empty())
      {
        std::string errmsg;
        errmsg = "node " + node + " not in the database";
        return onldb_resp(-1,errmsg);
      }
      nodes n = res[0];
      return onldb_resp(1,n.state);
    }
    else
    {
      mysqlpp::Query query = onl->query();
      query << "select state from hwclusters where cluster=" << mysqlpp::quote << node;
      mysqlpp::StoreQueryResult res = query.store();
      if(res.empty())
      {
        std::string errmsg;
        errmsg = "node " + node + " not in the database";
        return onldb_resp(-1,errmsg);
      }
      clusterstates c = res[0];
      return onldb_resp(1,c.state);
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
}

onldb_resp onldb::set_state(std::string node, std::string state, unsigned int len) throw()
{
  if(node.substr(0,5) == "vgige")
  {
    return onldb_resp(1,(std::string)"success");
  }
  std::string old_state;
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select state from nodes where node=" << mysqlpp::quote << node;
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "node " + node + " not in the database";
      return onldb_resp(-1,errmsg);
    } 
    nodestates n = res[0];
    old_state = n.state;
    if(n.state != state && n.state != "testing")
    {
      mysqlpp::Query update = onl->query();
      update << "update nodes set state=" << mysqlpp::quote << state << " where node=" << mysqlpp::quote << node;
      update.execute();
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }

  if(old_state != "testing")
  {
    if(state == "repair" && old_state != "repair")
    {
      onldb_resp r = handle_special_state(state, node, len, false);
      if(r.result() <= 0)
      {
        return onldb_resp(r.result(), r.msg());
      }
    }
    else if(old_state == "repair" && state != "repair")
    {
      onldb_resp r = clear_special_state(old_state, state, node);
      if(r.result() <= 0)
      {
        return onldb_resp(r.result(), r.msg());
      }
    }
  }
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::get_node_from_cp(std::string cp) throw()
{
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select node from nodes where daemonhost=" << mysqlpp::quote << cp;
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "cp " + cp + " not in the database";
      return onldb_resp(-1,errmsg);
    }
   
    // assuming a one-to-one mapping b/w cp and node
    nodenames nn = res[0];
    return onldb_resp(1,(std::string)nn.node);
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
}

onldb_resp onldb::put_in_testing(std::string node, unsigned int len) throw()
{
  std::string old_state;
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select state from nodes where node=" << mysqlpp::quote << node;
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "node " + node + " not in the database";
      return onldb_resp(-1,errmsg);
    }
    nodestates n = res[0];
    old_state = n.state;
    if(old_state == "testing")
    {
      std::string errmsg;
      errmsg = "node " + node + " already in testing";
      return onldb_resp(0, errmsg);
    }
    mysqlpp::Query update = onl->query();
    update << "update nodes set state='testing' where node=" << mysqlpp::quote << node;
    update.execute();
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }

  if(old_state == "repair")
  {
    onldb_resp r = clear_special_state(old_state, "testing", node);
    if(r.result() <= 0)
    {
      return onldb_resp(r.result(), r.msg());
    }
  }

  onldb_resp t = handle_special_state("testing", node, len, false);
  if(t.result() <= 0)
  {
    return onldb_resp(t.result(), t.msg());
  }

  return onldb_resp(1, (std::string)"success");
}

onldb_resp onldb::remove_from_testing(std::string node) throw()
{
  std::string old_state;
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select state from nodes where node=" << mysqlpp::quote << node;
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "node " + node + " not in the database";
      return onldb_resp(-1,errmsg);
    }
    nodestates n = res[0];
    old_state = n.state;
    if(old_state != "testing")
    {
      std::string errmsg;
      errmsg = "node " + node + " is not in testing";
      return onldb_resp(0, errmsg);
    }
    mysqlpp::Query update = onl->query();
    update << "update nodes set state='free' where node=" << mysqlpp::quote << node;
    update.execute();
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }

  onldb_resp r = clear_special_state(old_state, "free", node);
  if(r.result() <= 0)
  {
    return onldb_resp(r.result(), r.msg());
  }
  
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::extend_repair(std::string node, unsigned int len) throw()
{
  onldb_resp r = handle_special_state("repair", node, len, true);
  return onldb_resp(r.result(), r.msg());
}

onldb_resp onldb::extend_testing(std::string node, unsigned int len) throw()
{
  onldb_resp r = handle_special_state("testing", node, len, true);
  return onldb_resp(r.result(), r.msg());
}

onldb_resp onldb::handle_special_state(std::string state, std::string node, unsigned int len, bool extend) throw()
{
  unsigned int divisor;
  // get the hour divisor from policy so that we can force times into discrete slots
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select value from policy where parameter='divisor'";
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "policy database error.. divisor is not in the database";
      return onldb_resp(-1,errmsg);
    }
    policyvals pv = res[0];
    divisor = pv.value;
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
  
  std::string begin, end;
  bool is_parent = false;
  std::string parent_node = "";

  if(!extend)
  {
    if(len == 0)
    {
      try
      {
        mysqlpp::Query query = onl->query();
        query << "select value from policy where parameter='repair_time'";
        mysqlpp::StoreQueryResult res = query.store();
        if(res.empty())
        {
          std::string errmsg;
          errmsg = "policy database error.. repair_time is not in the database";
          return onldb_resp(-1,errmsg);
        }
        policyvals pv = res[0];
        len = pv.value;
      }
      catch(const mysqlpp::Exception& er)
      {
        return onldb_resp(-1,er.what());
      }
    }

    topology t;
    std::string type;
    unsigned int parent_label = 0;
    try
    {
      mysqlpp::Query query = onl->query();
      query << "select nodes.node,nodes.tid,hwclustercomps.cluster from nodes left join hwclustercomps using (node) where nodes.node=" << mysqlpp::quote << node;
      mysqlpp::StoreQueryResult res = query.store();
      if(res.empty()) return onldb_resp(-1,(std::string)"database error");
      specialnodeinfo sni = res[0];
      type = sni.tid;
      if(!sni.cluster.is_null)
      {
        parent_label = 2;
        onldb_resp pt = get_type(sni.cluster.data, true);
        if(pt.result() < 1) return onldb_resp(pt.result(), pt.msg());
        std::string parent_type = pt.msg();
        t.add_node(parent_type, parent_label, 0);
        is_parent = true;
        parent_node = sni.cluster.data;

        mysqlpp::Query query2 = onl->query();
        query2 << "select nodes.node,nodes.tid from nodes where node in (select node from hwclustercomps where node!=" << mysqlpp::quote << node << " and cluster=" << mysqlpp::quote << parent_node << ")";
        vector<specnodeinfo> complist;
        vector<specnodeinfo>::iterator comp;
        query2.storein(complist);
        unsigned int next_label = 3;
        for(comp = complist.begin(); comp != complist.end(); ++comp)
        {
          t.add_node(comp->tid,next_label,parent_label);
          onldb_resp blah = fix_component(&t, next_label, comp->node);
          if(blah.result() < 1)
          {
            return onldb_resp(blah.result(),blah.msg());
          }
          ++next_label;
          mysqlpp::Query update = onl->query();
          update << "update nodes set state=" << mysqlpp::quote << state << "where node=" << mysqlpp::quote << comp->node;
          update.execute();
        }
      }
    }
    catch(const mysqlpp::Exception& er)
    {
      return onldb_resp(-1,er.what());
    }

    t.add_node(type, 1, parent_label);
    onldb_resp fc = fix_component(&t, 1, node);
    if(fc.result() < 1)
    {
      return onldb_resp(fc.result(),fc.msg());
    }

    if(state == "repair")
    {
      char cmd[128];
      sprintf(cmd, "/usr/testbed/scripts/send_repair_email.pl %s %u", node.c_str(), len);
      int ret = system(cmd);
      if(ret < 0 || WEXITSTATUS(ret) != 1) cout << "Warning: " << node << " was put into repair but email was not sent" << endl;
    }
  
    time_t current_time_unix = time(NULL);
    current_time_unix = discretize_time(current_time_unix, divisor);
    begin = time_unix2db(current_time_unix);
    time_t end_time_unix = add_time(current_time_unix, len*60*60);
    end = time_unix2db(end_time_unix);

    if(lock("reservation") == false) return onldb_resp(0,"database locking problem");

    onldb_resp vcr = verify_clusters(&t);
    if(vcr.result() != 1)
    {
      unlock("reservation");
      return onldb_resp(vcr.result(),vcr.msg());
    }
    
    onldb_resp r = add_reservation(&t, state, begin, end);
    if(r.result() < 1)
    {
      unlock("reservation");
      return onldb_resp(r.result(), r.msg());
    }
  }
  else
  {
    if(len == 0)
    {
      return onldb_resp(0,"zero hours doesn't make sense");
    }

    time_t current_time_unix = time(NULL);
    current_time_unix = discretize_time(current_time_unix, divisor);
    std::string current_time_db = time_unix2db(current_time_unix);
    
    if(lock("reservation") == false) return onldb_resp(0,"database locking problem");

    time_t end_unix, new_end_unix;
    std::string end_db, new_end_db;
    unsigned int rid;
    try
    {
      mysqlpp::Query query = onl->query();
      query << "select nodes.node,nodes.tid,hwclustercomps.cluster from nodes left join hwclustercomps using (node) where nodes.node=" << mysqlpp::quote << node;
      mysqlpp::StoreQueryResult res = query.store();
      if(res.empty()) return onldb_resp(-1,(std::string)"database error");
      specialnodeinfo sni = res[0];
      if(!sni.cluster.is_null)
      {
        is_parent = true;
        parent_node = sni.cluster.data;
      }

      query = onl->query();
      if(is_parent)
      {
        query << "select begin,end,rid from reservations where user=" << mysqlpp::quote << state << " and begin<=" << mysqlpp::quote << current_time_db << " and end>=" << mysqlpp::quote << current_time_db << " and state!='timedout' and state!='cancelled' and rid in (select rid from hwclusterschedule where cluster=" << mysqlpp::quote << parent_node << ")";
      }
      else
      {
        query << "select begin,end,rid from reservations where user=" << mysqlpp::quote << state << " and begin<=" << mysqlpp::quote << current_time_db << " and end>=" << mysqlpp::quote << current_time_db << " and state!='timedout' and state!='cancelled' and rid in (select rid from nodeschedule where node=" << mysqlpp::quote << node << ")";
      }
      res = query.store();
      if(res.empty())
      {
        std::string errmsg;
        errmsg = "this is no current " + state + " reservation for " + node;
        unlock("reservation");
        return onldb_resp(0,errmsg);
      }
      curresinfo curres = res[0];
      end_unix = time_db2unix(curres.end);
      end_db = time_unix2db(end_unix);
      rid = curres.rid;
    }
    catch(const mysqlpp::Exception& er)
    {
      unlock("reservation");
      return onldb_resp(-1,er.what());
    }

    new_end_unix = add_time(end_unix, len*60*60);
    new_end_db = time_unix2db(new_end_unix);

    topology t;
    onldb_resp r = get_topology(&t, rid);
    if(r.result() < 1)
    {
      unlock("reservation");
      return onldb_resp(r.result(),r.msg());
    }

    try
    {
      mysqlpp::Query up = onl->query();
      up << "update reservations set end=" << mysqlpp::quote << new_end_db << " where rid=" << mysqlpp::quote << rid;
      up.execute();
    }
    catch(const mysqlpp::Exception& er)
    {
      unlock("reservation");
      return onldb_resp(-1,er.what());
    }

    begin = end_db;
    end = new_end_db;
  }

  vector<reservations> reslist;
  vector<reservations>::iterator res;
  try
  {
    mysqlpp::Query query = onl->query();
    if(is_parent)
    {
      query << "select rid,user,begin,end,state from reservations where end>=" << mysqlpp::quote << begin << " and begin<=" << mysqlpp::quote << end << " and state!='cancelled' and state!='timedout' and state!='active' and user!='repair' and user!='testing' and user!='system' and rid in (select rid from hwclusterschedule where cluster=" << mysqlpp::quote << parent_node << ")";
    }
    else
    {
      query << "select rid,user,begin,end,state from reservations where end>=" << mysqlpp::quote << begin << " and begin<=" << mysqlpp::quote << end << " and state!='cancelled' and state!='timedout' and state!='active' and user!='repair' and user!='testing' and user!='system' and rid in (select rid from nodeschedule where node=" << mysqlpp::quote << node << ")";
    }
    query.storein(reslist);
    for(res = reslist.begin(); res != reslist.end(); ++res)
    {
      mysqlpp::Query can = onl->query();
      can << "update reservations set state='cancelled' where rid=" << mysqlpp::quote << res->rid;
      can.execute();

      topology existing_top;
      onldb_resp r = get_topology(&existing_top, res->rid);
      // if something went wrong, this res is hosed anyway, so leave it cancelled
      if(r.result() < 1) continue;

      r = verify_clusters(&existing_top);
      // if something went wrong, this res is hosed anyway, so leave it cancelled
      if(r.result() < 1) continue;

      // clear out the existing assignments
      bool remap = true;
      std::list<node_resource_ptr>::iterator nit;
      for(nit = existing_top.nodes.begin(); nit != existing_top.nodes.end(); ++nit)
      {
        // if node is the node being handled, and it is fixed, then we can't remap this res
        if(((*nit)->node == parent_node || (*nit)->node == node) && (*nit)->fixed == true)
        {
          remap = false;
          break;
        }
        (*nit)->node = "";
        (*nit)->acl = "unused";
        (*nit)->cp = "unused";
      }
      std::list<link_resource_ptr>::iterator lit;
      for(lit = existing_top.links.begin(); lit != existing_top.links.end(); ++lit)
      {
        (*lit)->conns.clear();
      }

      if(remap)
      {
        std::string res_begin = time_unix2db(time_db2unix(res->begin));
        std::string res_end = time_unix2db(time_db2unix(res->end));
        //r = try_reservation(&existing_top, res->user, res_begin, res_end);//JP changed 3/29/12
        r = try_reservation(&existing_top, res->user, res_begin, res_end, "used");
        // if result is 1, then we made a new res for user, so go to next one
        if(r.result() == 1) continue;
        // again, if something bad happend, this res is probably hosed, so leave it cancelled
        if(r.result() < 0) continue;
      }

      // here, couldn't remap res, so need to uncancel original one
      mysqlpp::Query uncan = onl->query();
      uncan << "update reservations set state=" << mysqlpp::quote << res->state << " where rid=" << mysqlpp::quote << res->rid;
      uncan.execute();
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    unlock("reservation");
    return onldb_resp(-1,er.what());
  }
  
  unlock("reservation");
  return onldb_resp(1, "success");
}

onldb_resp onldb::clear_special_state(std::string state, std::string new_state, std::string node) throw()
{
  if(lock("reservation") == false) return onldb_resp(0,"database locking problem");
  try
  {
    std::string current_time = time_unix2db(time(NULL));

    bool is_parent = false;
    std::string parent_node = "";
    mysqlpp::Query query = onl->query();
    query << "select nodes.node,nodes.tid,hwclustercomps.cluster from nodes left join hwclustercomps using (node) where nodes.node=" << mysqlpp::quote << node;
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty()) return onldb_resp(-1,(std::string)"database error");
    specialnodeinfo sni = res[0];
    if(!sni.cluster.is_null)
    {
      is_parent = true;
      parent_node = sni.cluster.data;

      mysqlpp::Query query2 = onl->query();
      query2 << "select node,tid from nodes where node in (select node from hwclustercomps where node!=" << mysqlpp::quote << node << " and cluster=" << mysqlpp::quote << parent_node << ")";
      vector<specnodeinfo> complist;
      vector<specnodeinfo>::iterator comp;
      query2.storein(complist);
      for(comp = complist.begin(); comp != complist.end(); ++comp)
      {
        mysqlpp::Query update = onl->query();
        update << "update nodes set state=" << mysqlpp::quote << new_state << " where node=" << mysqlpp::quote << comp->node;
        update.execute();
      }
    }

    mysqlpp::Query can = onl->query();
    if(is_parent)
    {
      can << "update reservations set state='cancelled' where user=" << mysqlpp::quote << state << " and begin<" << mysqlpp::quote << current_time << " and end>" << mysqlpp::quote << current_time << " and (state='pending' or state='used') and rid in (select rid from hwclusterschedule where cluster=" << mysqlpp::quote << parent_node << ")";
    }
    else
    {
      can << "update reservations set state='cancelled' where user=" << mysqlpp::quote << state << " and begin<" << mysqlpp::quote << current_time << " and end>" << mysqlpp::quote << current_time << " and (state='pending' or state='used') and rid in (select rid from nodeschedule where node=" << mysqlpp::quote << node << ")";
    }
    can.execute();
  }
  catch(const mysqlpp::Exception& er)
  {
    unlock("reservation");
    return onldb_resp(-1,er.what());
  }
  unlock("reservation");
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::get_type(std::string node, bool is_cluster) throw()
{
  if(node.substr(0,5) == "vgige")
  {
    return onldb_resp(1,(std::string)"vgige");
  }
  try
  {
    if(!is_cluster)
    {
      mysqlpp::Query query = onl->query();
      query << "select tid from nodes where node=" << mysqlpp::quote << node;
      mysqlpp::StoreQueryResult res = query.store();
      if(res.empty())
      {
        std::string errmsg;
        errmsg = "node " + node + " not in the database";
        return onldb_resp(-1,errmsg);
      }
      typenameinfo nameinfo = res[0];
      return onldb_resp(1,nameinfo.tid);
    }
    else
    {
      mysqlpp::Query query = onl->query();
      query << "select tid from hwclusters where cluster=" << mysqlpp::quote << node;
      mysqlpp::StoreQueryResult res = query.store();
      if(res.empty())
      {
        std::string errmsg;
        errmsg = "node " + node + " not in the database";
        return onldb_resp(-1,errmsg);
      }
      typenameinfo nameinfo = res[0];
      return onldb_resp(1,nameinfo.tid);
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
}

onldb_resp onldb::authenticate_user(std::string username, std::string password_hash) throw()
{
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select password from users where user=" << mysqlpp::quote << username;
    mysqlpp::StoreQueryResult res = query.store(); 
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "user " + username + " not in the database";
      return onldb_resp(-1,errmsg);
    }
    passwords p = res[0];
    if(p.password == password_hash)
    {
      return onldb_resp(1,(std::string)"user authenticated");
    }
    else
    {
      return onldb_resp(0,(std::string)"password incorrect");
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
}

onldb_resp onldb::is_admin(std::string username) throw()
{
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select priv from users where user=" << mysqlpp::quote << username;
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "user " + username + " not in the database";
      return onldb_resp(-1,errmsg);
    }
    privileges p = res[0];
    if(p.priv > 0)
    {
      return onldb_resp(1,(std::string)"user is admin");
    }
    else
    {
      return onldb_resp(0,(std::string)"not admin");
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
}

onldb_resp onldb::reserve_all(std::string begin, unsigned int len) throw()
{
  unsigned int horizon;
  unsigned int divisor;

  time_t current_time_unix;
  time_t begin_unix;
  time_t end_unix;
  std::string current_time_db;
  std::string begin_db;
  std::string end_db;

  // get the hour divisor from policy so that we can force times into discrete slots
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select value from policy where parameter=" << mysqlpp::quote << "divisor";
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "policy database error.. divisor is not in the database";
      return onldb_resp(-1,errmsg);
    }
    policyvals pv = res[0];
    divisor = pv.value;
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }

  current_time_unix = time(NULL);
  current_time_unix = discretize_time(current_time_unix, divisor);
  current_time_db = time_unix2db(current_time_unix);

  begin_unix = time_db2unix(begin);
  begin_unix = discretize_time(begin_unix, divisor);
  begin_db = time_unix2db(begin_unix);

  if(begin_unix < current_time_unix)
  {
    std::string errmsg;
    errmsg = "begin time is in the past";
    return onldb_resp(0,errmsg);
  }

  if(len <= 0)
  {
    std::string errmsg;
    errmsg = "length of " + to_string(len) + " minutes does not make sense";
    return onldb_resp(0,errmsg);
  }

  unsigned int chunk = 60/divisor;
  len = (((len-1) / chunk)+1) * chunk;

  end_unix = add_time(begin_unix, len*60);
  end_db = time_unix2db(end_unix);

  // now start doing policy based checking
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select value from policy where parameter=" << mysqlpp::quote << "horizon";
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "policy database error.. horizon is not in the database";
      return onldb_resp(-1,errmsg);
    }
    policyvals pv = res[0];
    horizon = pv.value;
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }

  time_t horizon_limit_unix = add_time(current_time_unix, horizon*24*60*60);

  if(end_unix > horizon_limit_unix)
  {
    std::string errmsg;
    errmsg = "requested times too far into the future (currently a " + to_string(horizon) + " day limit)";
    return onldb_resp(0,errmsg);
  }

  if(lock("reservation") == false) return onldb_resp(0,"database locking problem.. try again later");

  try
  {
    mysqlpp::Query query = onl->query();
    query << "select user,begin,end from reservations where state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << end_db << " and end>" << mysqlpp::quote << begin_db << " order by begin";
    vector<otherrestimes> rts;
    query.storein(rts);
    vector<otherrestimes>::iterator restime;
    for(restime = rts.begin(); restime != rts.end(); ++restime)
    {
      if(restime->user != "testing" && restime->user != "repair")
      {
        unlock("reservation");
        return onldb_resp(0,(std::string)"there are non-system reservations during that time!");
      }
    }

    reservationins res("system", mysqlpp::DateTime(begin_db), mysqlpp::DateTime(end_db), "pending");
    mysqlpp::Query ins = onl->query();
    ins.insert(res);
    mysqlpp::SimpleResult sr = ins.execute();
    unsigned int rid = sr.insert_id();

    query = onl->query();
    query << "select cluster from hwclusters join types using(tid) where type='hwcluster' and cluster not in (select cluster from hwclusterschedule where rid in (select rid from reservations where begin<" << mysqlpp::quote << end_db << " and end>" << mysqlpp::quote << begin_db << " and state!='cancelled' and state!='testing'))";
    vector<clusternames> clusterlist;
    query.storein(clusterlist);
    vector<clusternames>::iterator clusterit;
    for(clusterit = clusterlist.begin(); clusterit != clusterlist.end(); ++clusterit)
    {
      hwclusterschedule hwcs(clusterit->cluster, rid, 1);
      mysqlpp::Query ins = onl->query();
      ins.insert(hwcs);
      ins.execute();
    }

    query = onl->query();
    query << "select node from nodes join types using(tid) where type='base' and node not in (select node from nodeschedule where rid in (select rid from reservations where begin<" << mysqlpp::quote << end_db << " and end>" << mysqlpp::quote << begin_db << " and state!='cancelled' and state!='testing'))";
    vector<nodenames> nodelist;
    query.storein(nodelist);
    vector<nodenames>::iterator nodeit;
    for(nodeit = nodelist.begin(); nodeit != nodelist.end(); ++nodeit)
    {
      nodeschedule ns(nodeit->node, rid, 1);
      mysqlpp::Query ins = onl->query();
      ins.insert(ns);
      ins.execute();
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    unlock("reservation");
    return onldb_resp(-1,er.what());
  }

  unlock("reservation");
  return onldb_resp(1,"success");
}

onldb_resp onldb::make_reservation(std::string username, std::string begin1, std::string begin2, unsigned int len, topology *t) throw()
{
  std::string tz;
  unsigned int horizon;
  unsigned int divisor;

  time_t current_time_unix;
  time_t begin1_unix;
  time_t begin2_unix;
  time_t end1_unix;
  time_t end2_unix;
  std::string current_time_db;
  std::string begin1_db;
  std::string begin2_db;
  std::string end1_db;
  std::string end2_db;

  vector<type_info_ptr> type_list;

  list<node_resource_ptr>::iterator hw;
  list<link_resource_ptr>::iterator link;
  vector<type_info_ptr>::iterator ti;

  // get the hour divisor from policy so that we can force times into discrete slots
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select value from policy where parameter=" << mysqlpp::quote << "divisor";
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "policy database error.. divisor is not in the database";
      return onldb_resp(-1,errmsg);
    }
    policyvals pv = res[0];
    divisor = pv.value;
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }

  current_time_unix = time(NULL);
  //set the current time to the start of the closest slot
  current_time_unix = discretize_time(current_time_unix, divisor);
  current_time_db = time_unix2db(current_time_unix);

  begin1_unix = time_db2unix(begin1);
  begin1_unix = discretize_time(begin1_unix, divisor);
  begin1_db = time_unix2db(begin1_unix);

  begin2_unix = time_db2unix(begin2);
  begin2_unix = discretize_time(begin2_unix, divisor);
  begin2_db = time_unix2db(begin2_unix);

std::string JDD="jdd";
std::string JP="jp";

  std::string demo1_begin = "20090814100000";
  std::string demo1_end = "20090814120000";

  std::string demo2_begin = "20090818030000";
  std::string demo2_end = "20090818090000";

  std::string demo3_begin = "20090827090000";
  std::string demo3_end = "20090827120000";

  time_t demo1_begin_unix = time_db2unix(demo1_begin);
  time_t demo1_end_unix = time_db2unix(demo1_end);
  time_t demo2_begin_unix = time_db2unix(demo2_begin);
  time_t demo2_end_unix = time_db2unix(demo2_end);
  time_t demo3_begin_unix = time_db2unix(demo3_begin);
  time_t demo3_end_unix = time_db2unix(demo3_end);

  // start with some basic sanity checking of arguments
  try
  {
    //check that a timezone is set for this user
    mysqlpp::Query query = onl->query();
    query << "select timezone from users where user=" << mysqlpp::quote << username;
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "user " + username + " not in the database";
      return onldb_resp(-1,errmsg);
    }
    timezones tzdata = res[0];
    tz = tzdata.timezone;
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
 
 //check beginning of start range is less than the end, i.e. we don't have a negative start window 
  if(begin1_unix > begin2_unix)
  {
    begin2_db = begin1_db;
    begin2_unix = begin1_unix;
  }
 
  //check if the start range is in the past 
  if(begin1_unix < current_time_unix && begin2_unix < current_time_unix)
  {
    std::string errmsg;
    errmsg = "range of begin times from " + begin1_db + " to " + begin2_db + " is in the past";
    return onldb_resp(0,errmsg);
  }
 
  //check if the beginning of the start range is in the past. if it is set the start range to begin at the current time 
  if(begin1_unix < current_time_unix)
  {
    begin1_db = current_time_db;
    begin1_unix = current_time_unix;
  }

  //check if length of the experiment is valid
  if(len <= 0)
  {
    std::string errmsg;
    errmsg = "length of " + to_string(len) + " minutes does not make sense";
    return onldb_resp(0,errmsg);
  }

  //chunk is the number of minutes in a time slot 
  unsigned int chunk = 60/divisor;
  //convert the length to be a number of minutes that is a multiple of the chunk
  len = (((len-1) / chunk)+1) * chunk;

  //check if no components have been specified
  if(t->nodes.size() == 0)
  {
    std::string errmsg;
    errmsg = "no components requested";
    return onldb_resp(0,errmsg);
  }

  // build a vector of type information for each type that is represented in the topology.
  // go through the topology, verify that the type is in the database and count the number of each type
  // this loop ends with a list type_list that has an entry for each type in the topology and 
  // the number of instances requested
  for(hw = t->nodes.begin(); hw != t->nodes.end(); ++hw)
  {
    std::string type_type = get_type_type((*hw)->type);
    //check if the type is valid
    if(type_type == "")
    {
      std::string errmsg;
      errmsg = "type " + (*hw)->type + " not in the database";
      return onldb_resp(-1,errmsg);
    }
    
    //if it's a cluster don't do anything
    if((*hw)->parent)
    {
      continue;
    }

    //if there is already an entry for the type in type_list increment the number of instances
    for(ti = type_list.begin(); ti != type_list.end(); ++ti)
    {
      if((*ti)->type == (*hw)->type)
      {
        ++((*ti)->num);
        break;
      }
    }
   
    //check if we found an entry for the type in type_list if so continue to the next node
    if(ti != type_list.end())
    {
      continue;
    }

    //if we get here then we found a type we hadn't seen before in the topology.
    //create a new entry for the type set the instance count to 1 and add it to type_list
    type_info_ptr new_type(new type_info());
    new_type->type = (*hw)->type;
    new_type->type_type = type_type;
    new_type->num = 1;
    new_type->grpmaxnum = 0;
    type_list.push_back(new_type);
  }

  //end1 is the first possible end time for the experiment if it started at begin1
  end1_unix = add_time(begin1_unix, len*60);
  end1_db = time_unix2db(end1_unix);

  //end2 is the last possible end time for the experiment if it started at begin2
  end2_unix = add_time(begin2_unix, len*60);
  end2_db = time_unix2db(end2_unix);

  // now start doing policy based checking
  try
  {
    //get the horizon from the policy object in the database.
    //return an error if the horizon is not found
    mysqlpp::Query query = onl->query();
    query << "select value from policy where parameter=" << mysqlpp::quote << "horizon";
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "policy database error.. horizon is not in the database";
      return onldb_resp(-1,errmsg);
    }
    policyvals pv = res[0];
    horizon = pv.value;
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }

  time_t horizon_limit_unix = add_time(current_time_unix, horizon*24*60*60);

  //first check that we're not asking for a reservation too far into the future 
  //return error if the first possible experiment end time is beyond the horizon
  if(end1_unix > horizon_limit_unix)
  {
    std::string errmsg;
    errmsg = "requested times too far into the future (currently a " + to_string(horizon) + " day limit)";
    return onldb_resp(0,errmsg);
  }

  //if the last possible experiment endtime is past the horizon
  //adjust the begin2 and end2 times  so that end2 = horizon and begin2 = horizon - length 
  if(end2_unix > horizon_limit_unix)
  {
    end2_unix = horizon_limit_unix;
    end2_db = time_unix2db(end2_unix);
    begin2_unix = sub_time(end2_unix, len*60);
    begin2_db = time_unix2db(begin2_unix);
  }

  //get the reservation lock
  if(lock("reservation") == false) return onldb_resp(0,"database locking problem.. try again later");

  // need to check that all components listed as being in clusters are actually in those clusters
  // and clusters are actual clusters
  onldb_resp vcr = verify_clusters(t);
  if(vcr.result() != 1)
  {
    unlock("reservation");
    return onldb_resp(vcr.result(),vcr.msg());
  }
 
  // based on subsequent checking, the time slots may be broken into discontinuous chunks, so maintain a
  // list of time ranges that are still possible. in almost every case, it will only be one entry, so the
  // overhead should be fairly minimal
  vector<time_range_ptr> possible_times;
  time_range_ptr orig_time(new time_range());
  //create a possible time range instance from the original time range and add it to the list of possible times 
  orig_time->b1_unix = begin1_unix;
  orig_time->b2_unix = begin2_unix;
  orig_time->e1_unix = end1_unix;
  orig_time->e2_unix = end2_unix;
  possible_times.push_back(orig_time);

  vector<time_range_ptr> new_possible_times;
  vector<time_range_ptr>::iterator tr;

  // do per-type checking
  // go through the type_list created earlier and check for per type policy violations
  for(ti = type_list.begin(); ti != type_list.end(); ++ti)
  {
    typepolicyvals tpv;
   
    try
    {
      //check if user is allowed to use this type
      mysqlpp::Query query = onl->query();
      query << "select maxlen,usermaxnum,usermaxusage,grpmaxnum,grpmaxusage from typepolicy where tid=" << mysqlpp::quote << (*ti)->type << " and begin<" << mysqlpp::quote << current_time_db << " and end>" << mysqlpp::quote << current_time_db << " and grp in (select grp from members where user=" << mysqlpp::quote << username << " and prime=1)";
      mysqlpp::StoreQueryResult res = query.store();
      if(res.empty())
      {
        std::string errmsg;
        errmsg = "you do not have access to type " + (*ti)->type;
        unlock("reservation");
        return onldb_resp(0,errmsg);
      }
      tpv = res[0];
    }
    catch(const mysqlpp::Exception& er)
    {
      unlock("reservation");
      return onldb_resp(-1,er.what());
    }
 
    //check if request exceeds maximum length allowed for this type 
    if(len > (unsigned int)(tpv.maxlen*60))
    {
      std::string errmsg;
      errmsg = "requested time too long (currently a " + to_string(tpv.maxlen) + " hour limit)";
      unlock("reservation");
      return onldb_resp(0,errmsg);
    }

    //check if request violates the maximum number a user can request of this time at one time
    if((*ti)->num > tpv.usermaxnum)
    {
      std::string errmsg;
      errmsg = "you do not have access to that many " + (*ti)->type + "s (currently a " + to_string(tpv.usermaxnum) + " limit)";
      unlock("reservation");
      return onldb_resp(0,errmsg);
    }

    //now go through list of possible times and check for any times that would violate the type's weekly usage policy (either per group or per user)
    for(tr = possible_times.begin(); tr != possible_times.end(); ++tr)
    {
      //find the start of the week for the first possible start time b1 and the end of the week for the last possible end time e2
      time_t week_start = get_start_of_week((*tr)->b1_unix);
      time_t last_week_start = get_start_of_week((*tr)->e2_unix);
      time_t end_week = add_time(last_week_start,60*60*24*7);
      while(week_start != end_week)
      {
        //look at one week at a time
        time_t this_week_end = add_time(week_start,60*60*24*7);

        int user_usage = 0;
        int grp_usage = 0;
        try
        {
          //ask for the users reservations for the week that use this type
          mysqlpp::Query query = onl->query();
          std::string week_start_db = time_unix2db(week_start);
          std::string week_end_db = time_unix2db(this_week_end);
          if((*ti)->type_type == "hwcluster")
          {
            query << "select begin,end from reservations where user=" << mysqlpp::quote << username << " and state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << week_end_db << " and end> " << mysqlpp::quote << week_start_db << " and rid in ( select hwclusterschedule.rid from hwclusterschedule,hwclusters where hwclusters.tid=" << mysqlpp::quote << (*ti)->type << " and hwclusterschedule.cluster=hwclusters.cluster )";
          }
          else
          {
            query << "select begin,end from reservations where user=" << mysqlpp::quote << username << " and state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << week_end_db << " and end> " << mysqlpp::quote << week_start_db << " and rid in ( select nodeschedule.rid from nodeschedule,nodes where nodes.tid=" << mysqlpp::quote << (*ti)->type << " and nodeschedule.node=nodes.node )";
          }
          vector<restimes> rts;
          query.storein(rts);
    
          vector<restimes>::iterator restime;
          //figure out the amount of time in the week used by each reservation 
          //and add it to the week's total user_usage
          for(restime = rts.begin(); restime != rts.end(); ++restime)
          {
            time_t rb = time_t(restime->begin);
            if(rb < week_start) { rb = week_start; }
            time_t re = time_t(restime->end);
            if(re > this_week_end) { re = this_week_end; }
            user_usage += (re-rb);
          }
          rts.clear();

          onl->query();
          //now ask for reservations for this type, week and group
          if((*ti)->type_type == "hwcluster")
          {
            query << "select begin,end from reservations where state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << week_end_db << " and end> " << mysqlpp::quote << week_start_db << " and rid in ( select hwclusterschedule.rid from hwclusterschedule,hwclusters where hwclusters.tid=" << mysqlpp::quote << (*ti)->type << " and hwclusterschedule.cluster=hwclusters.cluster ) and user in ( select user from members where prime=1 and grp in (select grp from members where prime=1 and user=" << mysqlpp::quote << username << "))";
          }
          else
          {
            query << "select begin,end from reservations where state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << week_end_db << " and end> " << mysqlpp::quote << week_start_db << " and rid in ( select nodeschedule.rid from nodeschedule,nodes where nodes.tid=" << mysqlpp::quote << (*ti)->type << " and nodeschedule.node=nodes.node ) and user in ( select user from members where prime=1 and grp in (select grp from members where prime=1 and user=" << mysqlpp::quote << username << "))";
          }
          query.storein(rts);

          //figure out the amount of time in the week used by each reservation 
          //and add it to the week's total group_usage
          for(restime = rts.begin(); restime != rts.end(); ++restime)
          {
            time_t rb = time_t(restime->begin);
            if(rb < week_start) { rb = week_start; }
            time_t re = time_t(restime->end);
            if(re > this_week_end) { re = this_week_end; }
            grp_usage += (re-rb);
          }

        }
        catch(const mysqlpp::Exception& er)
        {
          unlock("reservation");
          return onldb_resp(-1,er.what());
        }
       
        //figure out the maximum amount of time in the considered week, the requested experiment could use if it
        //was scheduled in this possible time range 
        int max_week_length;
        if((*tr)->b1_unix < week_start && (*tr)->e2_unix > this_week_end)
        {
          max_week_length = std::min(len, (unsigned int)7*24*60);
        }
        else if((*tr)->b1_unix < week_start)
        {
          max_week_length = std::min(len, (unsigned int)((*tr)->e2_unix - week_start)/60);
        }
        else if((*tr)->e2_unix > this_week_end)
        {
          max_week_length = std::min(len, (unsigned int)(this_week_end - (*tr)->b1_unix)/60);
        }
        else
        {
          max_week_length = len;
        }
        //calculate the new potential user and group usage for the week if an experiment
        //was scheduled in this possible range
        int new_user_usage = (user_usage/60) + (max_week_length * ((*ti)->num));
        int new_grp_usage = (grp_usage/60) + (max_week_length * ((*ti)->num));

        // if the usage would be over the limit, then remove this week from the possible times,
        // potentially with some stuff at the beginning and end if some hours were left for the week
        if(new_user_usage > (tpv.usermaxusage*60) || new_grp_usage > (tpv.grpmaxusage*60))
        {
          int user_slack_seconds = (tpv.usermaxusage*60*60) - user_usage;
          int grp_slack_seconds = (tpv.grpmaxusage*60*60) - grp_usage;
          int slack_seconds = std::min(user_slack_seconds, grp_slack_seconds);
          if(slack_seconds < 0) { slack_seconds = 0; }
          time_t adjusted_week_start = add_time(week_start,slack_seconds);
          time_t adjusted_week_end = sub_time(this_week_end,slack_seconds);
          if((*tr)->b1_unix < adjusted_week_start)
          {
            if(adjusted_week_start >= (*tr)->e1_unix)
            {
              time_range_ptr new_time(new time_range());
              new_time->b1_unix = (*tr)->b1_unix;
              new_time->e1_unix = (*tr)->e1_unix;
              new_time->b2_unix = sub_time((*tr)->b2_unix, ((*tr)->e2_unix) - adjusted_week_start);
              new_time->e2_unix = adjusted_week_start;
              new_possible_times.push_back(new_time);
            }
          }
          if((*tr)->e2_unix > adjusted_week_end)
          {
            if(adjusted_week_end <= (*tr)->b2_unix)
            {
              (*tr)->e1_unix = add_time((*tr)->e1_unix, (adjusted_week_end - (*tr)->b1_unix));
              (*tr)->b1_unix = adjusted_week_end;
            }
          }
        }
        else if((*tr)->e2_unix < this_week_end)
        {
          time_range_ptr new_time(new time_range());
          new_time->b1_unix = (*tr)->b1_unix;
          new_time->e1_unix = (*tr)->e1_unix;
          new_time->b2_unix = (*tr)->b2_unix;
          new_time->e2_unix = (*tr)->e2_unix;
          new_possible_times.push_back(new_time);
        }

        week_start = add_time(week_start,60*60*24*7);
      }
    }

    possible_times.clear();
    possible_times = new_possible_times;
    new_possible_times.clear();
    
    // grpmaxnum has to be done on a per time slot basis, so save it here for use later
    (*ti)->grpmaxnum = tpv.grpmaxnum;
  }

  //if no possible times are left then either the group or user usage policy would be violated 
  //by a reservation in the requested range.
  if(possible_times.size() == 0)
  {
    unlock("reservation");
    return onldb_resp(0,(std::string)"your usage during that time period is already maxed out");
  }


  if (username == JDD || username == JP) {
    cout << "Warning: Making reservation for " << username << ": testing for overlap" << endl;
  }
  // now go through each time frame and remove any time where the user already has a reservation
  for(tr = possible_times.begin(); tr != possible_times.end(); ++tr)
  {

    try
    {
      //first get the list of the user's reservations which overlap the possible range
      //i.e. all of the user's reservations that begin before the last possible end time e2
      //and end after the first possible start time b1
      mysqlpp::Query query = onl->query();
      std::string b1_db = time_unix2db((*tr)->b1_unix);
      std::string e2_db = time_unix2db((*tr)->e2_unix);
      query << "select begin,end from reservations where user=" << mysqlpp::quote << username << " and state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << e2_db << " and end>" << mysqlpp::quote << b1_db << " order by begin";
      vector<restimes> rts;
      query.storein(rts);
 
      //if there are no overlapping reservations in this range then add this range to the new list of possible ranges 
      if(rts.empty())
      {
        if (username == JDD || username == JP) {
          cout << "Warning: " << username << ": found no reservations for: b1_db: " << b1_db << "e2_db: " << e2_db << endl;
        }
        time_range_ptr new_time(new time_range());
        new_time->b1_unix = (*tr)->b1_unix;
        new_time->e1_unix = (*tr)->e1_unix;
        new_time->b2_unix = (*tr)->b2_unix;
        new_time->e2_unix = (*tr)->e2_unix;
        new_possible_times.push_back(new_time);
      }
      else //otw we found some overlapping reservations
      {
        // each time in here overlaps the time range in tr
        bool add_left_over = false;
        vector<restimes>::iterator restime;
        if (username == JDD || username == JP) {
        cout << "Warning: " << username << ": found reservations for: b1_db: " << b1_db << "e2_db: " << e2_db << endl;}
       
        //look at each overlapping reservation and find any unused portions of the possible range that we 
        //could schedule the experiment 
        for(restime = rts.begin(); restime != rts.end(); ++restime)
        {
          // JDD: Added 9/9/10: seems like add_left_over needs to be reset each time we iterate in this loop
          // JDD: That way only if something is left at the very end is it an acceptable time.
          //
          //add_left_over = false;

          // JDD: restime is an existing reservation time
          // JDD: rb is the begin time of current reservation we are testing against.
          // JDD: re is the  end  time of current reservation we are testing against.
          // JDD: tr is a possible time frame -- still not sure exactly what that means.
          time_t rb = time_t(restime->begin);
          time_t re = time_t(restime->end);
          if (username == JDD || username == JP) {
            cout << "Warning: " << username << ": testing restime->begin: " << restime->begin << "restime->end: " << restime->end << endl;
            {
              std::string s = time_unix2db((*tr)->b1_unix);
              std::string e = time_unix2db((*tr)->e1_unix);
              cout << "Warning: " << username << ": testing (*tr)->b1_unix: " << s << "(*tr)->e1_unix: " << e << endl;
            }
            {
              std::string s = time_unix2db((*tr)->b2_unix);
              std::string e = time_unix2db((*tr)->e2_unix);
              cout << "Warning: " << username << ": testing (*tr)->b2_unix: " << s << "(*tr)->e2_unix: " << e << endl;
            }
          }
 
          // if the reservation begin time is after both the begin and end time of the time frame (tr) being checked,
          // there is some time before the reservation that is a possible time to schedule the experiment
          if((*tr)->b1_unix < rb && rb >= (*tr)->e1_unix)
          {
            time_range_ptr new_time(new time_range());
            //create a new possible time range with the same b1 and e1 as the time range considered
            //the last possible end time e2 = the start of the reservation and b2 = the start of the reservation - length
            new_time->b1_unix = (*tr)->b1_unix;
            new_time->e1_unix = (*tr)->e1_unix;
            new_time->b2_unix = sub_time((*tr)->b2_unix, ((*tr)->e2_unix) - rb);
            new_time->e2_unix = rb;
            if (username == JDD || username == JP) {
              cout << "Warning: " << username << ": calling new_possible_times.push_back()" << endl;
            }
            //add the new time range to the new list of possible times
            new_possible_times.push_back(new_time);
          }
          // if the reservation ends before the last end time e2 or the last begin time of the time frame (tr) being checked,
          // there is some time after the reservation that is a possible time to schedule the experiment
          if((*tr)->e2_unix > re && re <= (*tr)->b2_unix)
          {
            //change the current time range currently considered to have the first possible start time (b1) be the 
            //end of the reservation (re) and the first possible end time (e1) be the end of the reservation plus 
            //the length of the experiment
            (*tr)->e1_unix = add_time((*tr)->e1_unix, (re - (*tr)->b1_unix));
            (*tr)->b1_unix = re;
            if (username == JDD || username == JP) {
              cout << "Warning: " << username << ": setting add_left_over = true" << endl;
              {
                std::string s = time_unix2db((*tr)->b1_unix);
                std::string e = time_unix2db((*tr)->e1_unix);
                cout << "Warning: " << username << ": NEW (*tr)->b1_unix: " << s << "(*tr)->e1_unix: " << e << endl;
              }
            }
            //mark this to add after we've considered all of the reservations
            //we may find a later reservation that makes this range no longer possible
            add_left_over = true;
          }
         //JP: added 11_21_11 to fix duplicate reservations
         //if the reservation begins before the first possible end (e1) for the range and ends after the last possible start
         //if the reservation we're testing makes the current range unusable there's no point looking at the rest of the reservations
         if ((*tr)->e1_unix >= rb && (*tr)->b2_unix < re)
         {
           //if add_left_over was set to true at this point, we need to set add_left_over to false
           //so we don't accidentally add a range that's unusable
           add_left_over = false;
           break;
         }
        }//end of loop testing overlapping reservations for the user

        //if the possible range was modified while processing the overlapping requests
        //and the range is still viable we should add it to the new possible times
        if(add_left_over)
        {
          time_range_ptr new_time(new time_range());
          new_time->b1_unix = (*tr)->b1_unix;
          new_time->e1_unix = (*tr)->e1_unix;
          new_time->b2_unix = (*tr)->b2_unix;
          new_time->e2_unix = (*tr)->e2_unix;
          if (username == JDD || username == JP) {
            cout << "Warning: " << username << ": in if (add_left_over) calling new_possible_times.push_back() " << endl;
          }
          new_possible_times.push_back(new_time);
        }
      }
    }
    catch(const mysqlpp::Exception& er)
    {
      unlock("reservation");
      return onldb_resp(-1,er.what());
    }
  }

  possible_times.clear();
  possible_times = new_possible_times;
  new_possible_times.clear();

  //if we don't have any possible times left then the user already has blocking reservations during
  //the requested time period    
  if(possible_times.size() == 0)
  {
    unlock("reservation");
    return onldb_resp(0,(std::string)"you already have reservations during that time period");
  }

  // don't forget to check grpmaxnum from typepolicy..
  
  //std::cout << "current db time: " << current_time_db << std::endl << std::endl;
  //iterate through the possible time ranges to see if we can make a reservation
  for(tr = possible_times.begin(); tr != possible_times.end(); ++tr) 
  {
    //std::cout << "begin1 db time: " << time_unix2db((*tr)->b1_unix) << std::endl;
    //std::cout << "begin2 db time: " << time_unix2db((*tr)->b2_unix) << std::endl;
    //std::cout << "end1 db time: " << time_unix2db((*tr)->e1_unix) << std::endl;
    //std::cout << "end2 db time: " << time_unix2db((*tr)->e2_unix) << std::endl << std::endl;


    //get a list of any reservations overlapping the current time range.
    //i.e. reservations that begin before the last possible end(e2) and end after the first possible begin(b1)
    mysqlpp::Query query = onl->query();
    std::string b1_db = time_unix2db((*tr)->b1_unix);
    std::string e2_db = time_unix2db((*tr)->e2_unix);
    query << "select begin,end from reservations where state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << e2_db << " and end>" << mysqlpp::quote << b1_db << " order by begin";
    vector<restimes> rts;
    query.storein(rts);

    //go through all the overlapping reservations and mark the beginning and end of each reservation as a point of interest
    //in other words this is where resource availability might change freeing up resources that might not have been available before
    //or using resources that were available
    vector<restimes>::iterator restime;
    for(restime = rts.begin(); restime != rts.end(); ++restime)
    {
      time_t rb = time_t(restime->begin);
      time_t re = time_t(restime->end);
      if(rb > (*tr)->b1_unix && rb < (*tr)->e2_unix)
      { 
        (*tr)->times_of_interest.push_back(rb);
      }
      if(re > (*tr)->b1_unix && re < (*tr)->e2_unix)
      { 
        (*tr)->times_of_interest.push_back(re);
      }
    }

    (*tr)->times_of_interest.sort();
    (*tr)->times_of_interest.unique();

    std::list<time_t>::iterator toi_start = (*tr)->times_of_interest.begin();
    std::list<time_t>::iterator toi_end = (*tr)->times_of_interest.begin();
    //set the times of interest end iterator to the first time that is >= the first possible end time
    while(toi_end != (*tr)->times_of_interest.end() && *toi_end < (*tr)->e1_unix) ++toi_end;
 
    time_t cur_start, cur_end;
    unsigned int increment = (60/divisor)*60;
    bool changed = true;
    //look at times starting at the first possible beginning time(b1) and incrementing by the time slot interval set by the policy
    //try to make a reservation for the first possible time and then each time we hit or pass a time of interest
    //return success if we successfully make a reservation
    //stop and return an error if we get to the end of the range and haven't successfully made a reservation
    for(cur_start = (*tr)->b1_unix, cur_end = (*tr)->e1_unix; cur_start <= (*tr)->b2_unix; cur_start = add_time(cur_start, increment), cur_end = add_time(cur_end, increment))
    {
      if (username == JDD || username == JP) {
        // JDD
        std::string s = time_unix2db(cur_start);
        std::string e = time_unix2db(cur_end);
        cout << "Warning: " << username << ": testing cur_start: " << s << "cur_end: " << e << endl;
      }
      if(cur_start < demo1_end_unix && cur_end > demo1_begin_unix &&
         username != "syscgw" && username != "wiseman"  && username != "sigcomm")
      {
        continue;
      }
      if(cur_start < demo2_end_unix && cur_end > demo2_begin_unix &&
         username != "syscgw" && username != "wiseman"  && username != "sigcomm")
      {
        continue;
      }
      if(cur_start < demo3_end_unix && cur_end > demo3_begin_unix &&
         username != "syscgw" && username != "wiseman"  && username != "sigcomm")
      {
        continue;
      }

      if(toi_start != (*tr)->times_of_interest.end() && *toi_start <= cur_start)
      //if(toi_start != (*tr)->times_of_interest.end() && *toi_start < cur_start) //JP changed to fix bug of reservation gaps
      {
        changed = true;
        ++toi_start;
      }
      if(toi_end != (*tr)->times_of_interest.end() && *toi_end <= cur_end)
      //if(toi_end != (*tr)->times_of_interest.end() && *toi_end < cur_end)//JP changed to fix bug of reservation gaps
      {
        changed = true;
        ++toi_end;
      }
      if(changed)
      {
        onldb_resp trr = try_reservation(t, username, time_unix2db(cur_start), time_unix2db(cur_end));
        if(trr.result() == 1)
        {
          std::string s = time_unix2db(cur_start);
          unlock("reservation");
          return onldb_resp(1,s);
        }
        if(trr.result() < 0)
        {
          unlock("reservation");
          return onldb_resp(-1,trr.msg());
        }
        changed = false;
      }
    }
  }

  unlock("reservation");
  return onldb_resp(0,(std::string)"too many resources used by others during those times");
}

onldb_resp onldb::extend_current_reservation(std::string username, int min) throw()
{
  unsigned int divisor;
  time_t current_time_unix;
  time_t begin_unix;
  time_t end_unix;
  std::string current_time_db;
  std::string begin_db;
  std::string end_db;
  unsigned int rid;
  time_t new_end_unix;
  std::string new_end_db;

  try
  {
    mysqlpp::Query query = onl->query();
    query << "select value from policy where parameter=" << mysqlpp::quote << "divisor";
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "policy database error.. divisor is not in the database";
      return onldb_resp(-1,errmsg);
    }
    policyvals pv = res[0];
    divisor = pv.value;
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }

  current_time_unix = time(NULL);
  current_time_unix = discretize_time(current_time_unix, divisor);
  current_time_db = time_unix2db(current_time_unix);

  if(min <= 0)
  {
    std::string errmsg;
    errmsg = "length of " + to_string(min) + " minutes does not make sense";
    return onldb_resp(0,errmsg);
  }
 
  unsigned int chunk = 60/divisor;
  min = (((min-1) / chunk)+1) * chunk;

  if(lock("reservation") == false) return onldb_resp(0,"database locking problem.. try again later");

  // get the existing reservation
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select begin,end,rid from reservations where user=" << mysqlpp::quote << username << " and begin<=" << mysqlpp::quote << current_time_db << " and end>=" << mysqlpp::quote << current_time_db << " and state!='timedout' and state!='cancelled'";
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg;
      errmsg = "you do not have a current reservation";
      unlock("reservation");
      return onldb_resp(0,errmsg);
    }
    curresinfo curres = res[0];
    begin_unix = time_db2unix(curres.begin);
    end_unix = time_db2unix(curres.end);
    begin_db = time_unix2db(begin_unix);
    end_db = time_unix2db(end_unix);
    rid = curres.rid;
  }
  catch(const mysqlpp::Exception& er)
  {
    unlock("reservation");
    return onldb_resp(-1,er.what());
  }
  
  new_end_unix = add_time(end_unix, min*60);
  new_end_db = time_unix2db(new_end_unix);
  
  unsigned int len = (new_end_unix - current_time_unix)/60;

  topology res_top;
  onldb_resp r = get_topology(&res_top, rid);
  if(r.result() < 1)
  {
    unlock("reservation");
    return onldb_resp(r.result(),r.msg());
  }

  vector<type_info_ptr> type_list;
  vector<type_info_ptr>::iterator ti;
  list<node_resource_ptr>::iterator hw;
  list<link_resource_ptr>::iterator link;

  // build a vector of type information for each type that is represented in the topology.
  for(hw = res_top.nodes.begin(); hw != res_top.nodes.end(); ++hw)
  {
    std::string type_type = get_type_type((*hw)->type);
    if(type_type == "")
    {
      std::string errmsg;
      errmsg = "type " + (*hw)->type + " not in the database";
      unlock("reservation");
      return onldb_resp(-1,errmsg);
    }

    if((*hw)->parent)
    {
      continue;
    }

    for(ti = type_list.begin(); ti != type_list.end(); ++ti)
    {
      if((*ti)->type == (*hw)->type)
      {
        ++((*ti)->num);
        break;
      }
    }
    if(ti != type_list.end())
    {
      continue;
    }

    type_info_ptr new_type(new type_info());
    new_type->type = (*hw)->type;
    new_type->type_type = type_type;
    new_type->num = 1;
    new_type->grpmaxnum = 0;
    type_list.push_back(new_type);
  }

  // do per-type checking
  for(ti = type_list.begin(); ti != type_list.end(); ++ti)
  {
    typepolicyvals tpv;
   
    try
    {
      mysqlpp::Query query = onl->query();
      query << "select maxlen,usermaxnum,usermaxusage,grpmaxnum,grpmaxusage from typepolicy where tid=" << mysqlpp::quote << (*ti)->type << " and begin<" << mysqlpp::quote << current_time_db << " and end>" << mysqlpp::quote << current_time_db << " and grp in (select grp from members where user=" << mysqlpp::quote << username << " and prime=1)";
      mysqlpp::StoreQueryResult res = query.store();
      if(res.empty())
      {
        std::string errmsg;
        errmsg = "you do not have access to type " + (*ti)->type;
        unlock("reservation");
        return onldb_resp(0,errmsg);
      }
      tpv = res[0];
    }
    catch(const mysqlpp::Exception& er)
    {
      unlock("reservation");
      return onldb_resp(-1,er.what());
    }

    if(len > (unsigned int)(tpv.maxlen*60))
    {
      std::string errmsg;
      errmsg = "requested time too long (currently a " + to_string(tpv.maxlen) + " hour limit)";
      unlock("reservation");
      return onldb_resp(0,errmsg);
    }

    time_t week_start = get_start_of_week(current_time_unix);
    time_t last_week_start = get_start_of_week(new_end_unix);
    time_t end_week = add_time(last_week_start,60*60*24*7);
    while(week_start != end_week)
    {
      time_t this_week_end = add_time(week_start,60*60*24*7);

      int user_usage = 0;
      int grp_usage = 0;
      try
      {
        mysqlpp::Query query = onl->query();
        std::string week_start_db = time_unix2db(week_start);
        std::string week_end_db = time_unix2db(this_week_end);
        if((*ti)->type_type == "hwcluster")
        {
          query << "select begin,end from reservations where user=" << mysqlpp::quote << username << " and state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << week_end_db << " and end> " << mysqlpp::quote << week_start_db << " and rid in ( select hwclusterschedule.rid from hwclusterschedule,hwclusters where hwclusters.tid=" << mysqlpp::quote << (*ti)->type << " and hwclusterschedule.cluster=hwclusters.cluster )";
        }
        else
        {
          query << "select begin,end from reservations where user=" << mysqlpp::quote << username << " and state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << week_end_db << " and end> " << mysqlpp::quote << week_start_db << " and rid in ( select nodeschedule.rid from nodeschedule,nodes where nodes.tid=" << mysqlpp::quote << (*ti)->type << " and nodeschedule.node=nodes.node )";
        }
        vector<restimes> rts;
        query.storein(rts);
        vector<restimes>::iterator restime;
        for(restime = rts.begin(); restime != rts.end(); ++restime)
        {
          time_t rb = time_t(restime->begin);
          if(rb < week_start) { rb = week_start; }
          time_t re = time_t(restime->end);
          if(re > this_week_end) { re = this_week_end; }
          user_usage += (re-rb);
        }
        rts.clear();

        onl->query();
        if((*ti)->type_type == "hwcluster")
        {
          query << "select begin,end from reservations where state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << week_end_db << " and end> " << mysqlpp::quote << week_start_db << " and rid in ( select hwclusterschedule.rid from hwclusterschedule,hwclusters where hwclusters.tid=" << mysqlpp::quote << (*ti)->type << " and hwclusterschedule.cluster=hwclusters.cluster ) and user in ( select user from members where prime=1 and grp in (select grp from members where prime=1 and user=" << mysqlpp::quote << username << "))";
        }
        else
        {
          query << "select begin,end from reservations where state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << week_end_db << " and end> " << mysqlpp::quote << week_start_db << " and rid in ( select nodeschedule.rid from nodeschedule,nodes where nodes.tid=" << mysqlpp::quote << (*ti)->type << " and nodeschedule.node=nodes.node ) and user in ( select user from members where prime=1 and grp in (select grp from members where prime=1 and user=" << mysqlpp::quote << username << "))";
        }
        query.storein(rts);
        for(restime = rts.begin(); restime != rts.end(); ++restime)
        {
          time_t rb = time_t(restime->begin);
          if(rb < week_start) { rb = week_start; }
          time_t re = time_t(restime->end);
          if(re > this_week_end) { re = this_week_end; }
          grp_usage += (re-rb);
        }
      }
      catch(const mysqlpp::Exception& er)
      {
        unlock("reservation");
        return onldb_resp(-1,er.what());
      }

      int max_week_length;
      if(current_time_unix < week_start && new_end_unix > this_week_end)
      {
        max_week_length = std::min(len, (unsigned int)7*24*60);
      }
      else if(current_time_unix < week_start)
      {
        max_week_length = std::min(len, (unsigned int)(new_end_unix - week_start)/60);
      }
      else if(new_end_unix > this_week_end)
      {
        max_week_length = std::min(len, (unsigned int)(this_week_end - current_time_unix)/60);
      }
      else
      {
        max_week_length = len;
      }
      int new_user_usage = (user_usage/60) + (max_week_length * ((*ti)->num));
      int new_grp_usage = (grp_usage/60) + (max_week_length * ((*ti)->num));

      if(new_user_usage > (tpv.usermaxusage*60) || new_grp_usage > (tpv.grpmaxusage*60))
      {
        unlock("reservation");
        return onldb_resp(0,(std::string)"your usage during this time period is already maxed out");
      }
      week_start = add_time(week_start,60*60*24*7);
    }
  }

  onldb_resp ra = is_admin(username);
  if(ra.result() < 0)
  {
    unlock("reservation");
    return onldb_resp(ra.result(),ra.msg());
  }
  bool admin = false;
  if(ra.result() == 1) admin = true;

  try
  {
    mysqlpp::Query query = onl->query();
    query << "select begin,end from reservations where user=" << mysqlpp::quote << username << " and state!='cancelled' and state!='timedout' and begin<=" << mysqlpp::quote << new_end_db << " and end>" << mysqlpp::quote << end_db;
    vector<restimes> rts;
    query.storein(rts);
  
    if(!rts.empty())
    {
      unlock("reservation");
      return onldb_resp(0,(std::string)"you already have a reservation during that time period");
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    unlock("reservation");
    return onldb_resp(-1,er.what());
  }

  for(hw = res_top.nodes.begin(); hw != res_top.nodes.end(); ++hw)
  {
    if((*hw)->parent) continue;
    if((*hw)->type == "vgige") continue;
    
    try
    {
      mysqlpp::Query query = onl->query();
      if((*hw)->is_parent)
      {
        if(admin == true)
        {
          query << "select rid from hwclusterschedule where cluster=" << mysqlpp::quote << (*hw)->node << " and rid in (select rid from reservations where state!='cancelled' and state!='timedout' and user!='system' and user!='testing' and user!='repair' and begin<=" << mysqlpp::quote << new_end_db << " and end>" << mysqlpp::quote << end_db << ")";
        }
        else
        {
          query << "select rid from hwclusterschedule where cluster=" << mysqlpp::quote << (*hw)->node << " and rid in (select rid from reservations where state!='cancelled' and state!='timedout' and begin<=" << mysqlpp::quote << new_end_db << " and end>" << mysqlpp::quote << end_db << ")";
        }
      }
      else
      {
        if(admin == true)
        {
          query << "select rid from nodeschedule where node=" << mysqlpp::quote << (*hw)->node << " and rid in (select rid from reservations where state!='cancelled' and state!='timedout' and user!='system' and user!='testing' and user!='repair' and begin<=" << mysqlpp::quote << new_end_db << " and end>" << mysqlpp::quote << end_db << ")";
        }
        else
        {
          query << "select rid from nodeschedule where node=" << mysqlpp::quote << (*hw)->node << " and rid in (select rid from reservations where state!='cancelled' and state!='timedout' and begin<=" << mysqlpp::quote << new_end_db << " and end>" << mysqlpp::quote << end_db << ")";
        }
      }
      mysqlpp::StoreQueryResult res = query.store();
      if(!res.empty())
      {
        unlock("reservation");
        return onldb_resp(0,(std::string)"resources used by others during that time");
      }
    }
    catch(const mysqlpp::Exception& er)
    {
      unlock("reservation");
      return onldb_resp(-1,er.what());
    }
  }
  
  // check capacity along all switch->switch links to make sure there's enough capacity
  // to extend this res.  returns 1 if there is enough
  onldb_resp bwr = check_interswitch_bandwidth(&res_top, end_db, new_end_db);
  if(bwr.result() < 1)
  {
    unlock("reservation");
    return onldb_resp(bwr.result(),bwr.msg());
  }

  try
  {
    mysqlpp::Query up = onl->query();
    up << "update reservations set end=" << mysqlpp::quote << new_end_db << " where rid=" << mysqlpp::quote << rid;
    up.execute();
  }
  catch(const mysqlpp::Exception& er)
  {
    unlock("reservation");
    return onldb_resp(-1,er.what());
  }

  unlock("reservation");
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::fix_component(topology *t, unsigned int label, std::string node) throw()
{
  list<node_resource_ptr>::iterator nit;
  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    if((*nit)->label == label)
    {
      (*nit)->node = node;
      (*nit)->fixed = true;
      break;
    }
  }
  if(nit == t->nodes.end()) return onldb_resp(0,"no such label");

  if((*nit)->parent) 
  {
    std::string parent = "";
    try
    {
      mysqlpp::Query query = onl->query();
      query << "select cluster from hwclustercomps where node=" << mysqlpp::quote << node;
      mysqlpp::StoreQueryResult res = query.store();
      if(res.empty()) return onldb_resp(0,(std::string)"topology error");
      clustercluster c = res[0];
      parent = c.cluster;
    }
    catch(const mysqlpp::Exception& er)
    {
      return onldb_resp(-1,er.what());
    }

    (*nit)->parent->node = parent;
    (*nit)->parent->fixed = true;
  }

  return onldb_resp(1,"success");
}

onldb_resp onldb::cancel_current_reservation(std::string username) throw()
{
  if(lock("reservation") == false) return onldb_resp(0,"database locking problem.. try again later");
  try
  {
    std::string current_time = time_unix2db(time(NULL));
    mysqlpp::Query can = onl->query();
    can << "update reservations set state='cancelled' where user=" << mysqlpp::quote << username << " and begin<" << mysqlpp::quote << current_time << " and end>" << mysqlpp::quote << current_time << " and (state='pending' or state='used')";
    can.execute();
  }
  catch(const mysqlpp::Exception& er)
  {
    unlock("reservation");
    return onldb_resp(-1,er.what());
  }
  unlock("reservation");
  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::has_reservation(std::string username) throw()
{
  if(lock("reservation") == false) return onldb_resp(0,"database locking problem.. try again later");
  try
  {
    std::string current_time = time_unix2db(time(NULL));
    mysqlpp::Query has = onl->query();
    has << "select end from reservations where user=" << mysqlpp::quote << username << " and begin<" << mysqlpp::quote << current_time << " and end>" << mysqlpp::quote << current_time << " and (state='active' or state='pending' or state='used')";
    mysqlpp::StoreQueryResult res = has.store();
    if(res.empty())
    {
      unlock("reservation");
      return onldb_resp(-1,(std::string)"no reservation");
    }
    resend res_end = res[0];
    std::string end_time = res_end.end.str();
    unlock("reservation");
    int seconds_left = time_db2unix(end_time) - time_db2unix(current_time);
    if(seconds_left > 0) return onldb_resp(seconds_left/60,(std::string)"success");
    return onldb_resp(0,(std::string)"no time left");
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,er.what());
  }
}

onldb_resp onldb::assign_resources(std::string username, topology *t) throw()
{
  if(lock("reservation") == false) return onldb_resp(0,"database locking problem.. try again later");

  std::string current_time = time_unix2db(time(NULL));
  try
  {
    // first verify that the user has a reservation right now
    mysqlpp::Query query = onl->query();
    query << "select rid,state from reservations where user=" << mysqlpp::quote << username << " and begin<" << mysqlpp::quote << current_time << " and end>" << mysqlpp::quote << current_time << " and (state='pending' or state='active' or state='used')";
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg = "reservation not found";
      unlock("reservation");
      return onldb_resp(0,errmsg);
    }
    resinfo ri = res[0];

    // verify that the topology clusters are correct
    onldb_resp r1 = verify_clusters(t);
    if(r1.result() < 1)
    {
      unlock("reservation");
      return onldb_resp(0,r1.msg());
    }

    // now fill in the physical resources from the reservation 
    onldb_resp r2 = fill_in_topology(t, ri.rid);
    if(r2.result() < 1)
    {
      unlock("reservation");
      return onldb_resp(0,r2.msg());
    }

    // update the reservation state to show that it is active now
    resinfo orig_ri = ri;
    ri.state = "active";
    query.update(orig_ri, ri);
    query.execute();

    mysqlpp::DateTime ct(current_time);
    // add an experiment for this reservation
    experimentins exp(ct, ct, ri.rid);
    mysqlpp::Query ins = onl->query();
    ins.insert(exp);
    ins.execute();
  }
  catch(const mysqlpp::Exception& er)
  {
    unlock("reservation");
    return onldb_resp(-1,er.what());
  }
  unlock("reservation");

  // now update the soft system state, to add the user to the correct SSH ACLs
  // and export their home area to their nodes
  // note that the script checks for "unused" arguments and ignores them
  std::list<node_resource_ptr>::iterator nit;
#ifdef USE_EXPORTFS
  std::string exportList;
#endif
  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
#ifdef USE_EXPORTFS
    if (((*nit)->cp).compare("unused")) {
      exportList.append((*nit)->cp);
      exportList.append(":/users/");
      exportList.append(username);
      exportList.append(" ");
    }
#endif

    std::string cmd = "/usr/testbed/scripts/system_session_update2.pl add " + username + " " + (*nit)->cp + " " + (*nit)->acl;
    int ret = system(cmd.c_str());
    if(ret < 0 || WEXITSTATUS(ret) != 1) cout << "Warning: " << username << "'s home area was not exported to " << (*nit)->cp << " and user was not added to group " << (*nit)->acl << endl;
  }
#ifdef USE_EXPORTFS
  cout << "exportList: > " << exportList << " < " << endl;
#endif

#ifdef USE_EXPORTFS
  {
    std::string cmd = "/usr/sbin/exportfs -o rw,sync,no_root_squash " + exportList;
    int ret = system(cmd.c_str());
    if(ret < 0 || ((ret !=0) && WEXITSTATUS(ret) != 1)) cout << "Warning: " << " exportfs failed (ret = " << ret << ") for " << cmd << endl;
    else cout << "exportfs succeeded for " << cmd << endl;
  }
#endif
  {
    std::string cmd = "/usr/testbed/scripts/system_session_update2.pl update";
    int ret = system(cmd.c_str());
    if(ret < 0 || WEXITSTATUS(ret) != 1) cout << "Warning: " << username << " was not added to any groups" << endl;
  }

  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::return_resources(std::string username, topology *t) throw()
{
  if(lock("reservation") == false) return onldb_resp(0,"database locking problem.. try again later");

  std::string current_time = time_unix2db(time(NULL));
  try
  {
    // get the active experiment for this user's active reservation
    mysqlpp::Query query = onl->query();
    query << "select eid,begin,end,rid from experiments where begin=end and rid in (select rid from reservations where user=" << mysqlpp::quote << username << " and state='active')";
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      std::string errmsg = "experiment not found";
      unlock("reservation");
      return onldb_resp(-1,errmsg);
    }
    experiments exp = res[0];

    // update the experiment to have the correct endtime
    experiments orig_exp = exp;
    mysqlpp::sql_datetime cur_time(current_time);
    exp.end = cur_time;
    query.update(orig_exp, exp);
    query.execute();

    // update the reservation to be used
    mysqlpp::Query resup = onl->query();
    resup << "update reservations set state='used' where rid=" << mysqlpp::quote << orig_exp.rid;
    resup.execute();
  }
  catch(const mysqlpp::Exception& er)
  {
    unlock("reservation");
    return onldb_resp(-1,er.what());
  }
  unlock("reservation");

  // now remove all the soft system state that was added in assign_resources
  std::list<node_resource_ptr>::iterator nit;
#ifdef USE_EXPORTFS
  std::string exportList;
#endif
  for(nit = t->nodes.begin(); nit != t->nodes.end(); ++nit)
  {
    std::string cmd = "/usr/testbed/scripts/system_session_update2.pl remove " + username + " " + (*nit)->cp + " " + (*nit)->acl;
#ifdef USE_EXPORTFS
    if (((*nit)->cp).compare("unused")) {
      exportList.append((*nit)->cp);
      exportList.append(":/users/");
      exportList.append(username);
      exportList.append(" ");
    }
#endif
    int ret = system(cmd.c_str());
    if(ret < 0 || WEXITSTATUS(ret) != 1) cout << "Warning: " << username << "'s home area was not unexported to " << (*nit)->cp << " and user was not removed from group " << (*nit)->acl << endl;
  }
#ifdef USE_EXPORTFS
  cout << "exportList: > " << exportList << " < " << endl;
#endif
  {
    std::string cmd = "/usr/testbed/scripts/system_session_update2.pl update";
    int ret = system(cmd.c_str());
    if(ret < 0 || WEXITSTATUS(ret) != 1) cout << "Warning: " << username << " was not removed from any groups" << endl;
  }

#ifdef USE_EXPORTFS
  {
    std::string cmd = "/usr/sbin/exportfs -u " + exportList;
    int ret = system(cmd.c_str());
    if(ret < 0 || ((ret !=0) && WEXITSTATUS(ret) != 1)) cout << "Warning: " << " exportfs failed (ret = " << ret << ") for " << cmd << endl;
    else cout << "exportfs succeeded for " << cmd << endl;
  }
#endif

  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::get_expired_sessions(std::list<std::string>& users) throw()
{
  users.clear();

  std::string current_time = time_unix2db(time(NULL));

  try
  {
    mysqlpp::Query query = onl->query();
    query << "select user from reservations where end<=" << mysqlpp::quote << current_time << " and rid in (select rid from experiments where begin=end)";
    vector<usernames> expired_users;
    query.storein(expired_users);
    if(expired_users.empty())
    {
      return onldb_resp(1,(std::string)"success");
    }

    std::map<int, int> caps;
    std::map<int, int>::iterator mit;

    mysqlpp::Query query2 = onl->query();
    query << "select cid,capacity from connschedule where rid in (select rid from experiments where begin=end)";
    vector<capconninfo> cci;
    query.storein(cci);
    vector<capconninfo>::iterator conn;
    for(conn = cci.begin(); conn != cci.end(); ++conn)
    {
      if(conn->cid == 0) { continue; }
      if(caps.find(conn->cid) == caps.end())
      {
        caps[conn->cid] = conn->capacity;
      }
      else
      {
        caps[conn->cid] += conn->capacity;
      }
    }

    for(mit = caps.begin(); mit != caps.end(); ++mit)
    {
      mysqlpp::Query query3 = onl->query();
      query3 << "select capacity from connections where cid=" << mysqlpp::quote << mit->first;
      vector<capinfo> ci;
      query3.storein(ci);
      if(ci.size() != 1) { return onldb_resp(-1, (std::string)"database consistency problem"); }
      if(caps[mit->first] > ci[0].capacity)
      {
        vector<usernames>::iterator u;
        for(u = expired_users.begin(); u != expired_users.end(); ++u)
        {
          users.push_back(u->user);
        }
        return onldb_resp(1,(std::string)"success");
      }
    }
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,(std::string)"database consistency problem");
  }

  return onldb_resp(1,(std::string)"success");
}

onldb_resp onldb::get_capacity(std::string type, unsigned int port) throw()
{
  try
  {
    mysqlpp::Query query = onl->query();
    query << "select bandwidth from interfacetypes where interface in (select interface from interfaces where tid=" << mysqlpp::quote << type << " and port=" << mysqlpp::quote << port << ")";
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      return onldb_resp(-1,(std::string)"no such port");
    }
    bwinfo bw = res[0];
    return onldb_resp(bw.bandwidth,(std::string)"success");
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,(std::string)"no such port");
  }
}

onldb_resp onldb::get_switch_ports(unsigned int cid, switch_port_info& info1, switch_port_info& info2) throw()
{
  if(cid == 0)
  {
    info1 = switch_port_info("",0,false);
    info2 = switch_port_info("",0,false);
    return onldb_resp(1,(std::string)"success");
  }

  try
  {
    mysqlpp::Query query = onl->query();
    query << "select node1,node1port,node2,node2port from connections where cid=" << mysqlpp::quote << cid;
    mysqlpp::StoreQueryResult res = query.store();
    if(res.empty())
    {
      return onldb_resp(-1,(std::string)"no such port");
    }
    conninfo conn = res[0];

    bool n1inf = false;
    bool n2inf = false;

    onldb_resp r1 = is_infrastructure(conn.node1); 
    if(r1.result() < 0) return onldb_resp(-1, (std::string)"database consistency problem");
    if(r1.result() == 1) { n1inf = true; }
    
    onldb_resp r2 = is_infrastructure(conn.node2); 
    if(r2.result() < 0) return onldb_resp(-1, (std::string)"database consistency problem");
    if(r2.result() == 1) { n2inf = true; }
    
    std::string n1 = "";
    unsigned int n1p = 0;
    std::string n2 = "";
    unsigned int n2p = 0;
    bool inf_port = false;

    if(n1inf)
    {
      n1 = conn.node1;
      n1p = conn.node1port;
    }
    if(n2inf) 
    {
      n2 = conn.node2;
      n2p = conn.node2port;
    }
    if(n1inf && n2inf)
    {
      inf_port = true;
    }

    info1 = switch_port_info(n1,n1p,inf_port);
    info2 = switch_port_info(n2,n2p,inf_port);

    return onldb_resp(1,(std::string)"success");
  }
  catch(const mysqlpp::Exception& er)
  {
    return onldb_resp(-1,(std::string)"database consistency problem");
  }
}
