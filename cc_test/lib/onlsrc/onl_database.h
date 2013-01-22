#ifndef _ONL_DATABASE_H
#define _ONL_DATABASE_H

#include <boost/shared_ptr.hpp>

// going to put all forward declarations here for now
namespace mysqlpp
{
  class Connection;
};

namespace onl
{
  struct _node_resource;
  typedef struct _node_resource node_resource;
  typedef boost::shared_ptr<node_resource> node_resource_ptr;
  struct _link_resource;
  typedef struct _link_resource link_resource;
  typedef boost::shared_ptr<link_resource> link_resource_ptr;
  struct _assign_info;
  typedef struct _assign_info assign_info;
  typedef boost::shared_ptr<assign_info> assign_info_ptr;
};

#include <onldb_resp.h>
#include <topology.h>
#include <gurobi_c++.h>
#include <onldb.h>
#include <onltempdb.h>

#endif // _ONL_DATABASE_H
