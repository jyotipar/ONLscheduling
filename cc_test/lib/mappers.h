/*
 * Copyright (c) 2008-2013 Charlie Wiseman, Jyoti Parwatikar and Washington University in St. Louis.
 * All rights reserved
 *
 * Distributed under the terms of the GNU General Public License v3
 * */

#ifndef _MAPPERS_H
#define _MAPPERS_H

#include <boost/shared_ptr.hpp>

struct _node_resource;
typedef struct _node_resource node_resource;
typedef boost::shared_ptr<node_resource> node_resource_ptr;

struct _link_resource;
typedef struct _link_resource link_resource;
typedef boost::shared_ptr<link_resource> link_resource_ptr;

//JP added 11/7/2012
//struct _vswitch_group;
//typedef struct _vswitch_group vswitch_group;
//typedef boost::shared_ptr<vswitch_group> vswitch_group_ptr;

//JP added
struct _subnet_info;
typedef struct _subnet_info subnet_info;
typedef boost::shared_ptr<subnet_info> subnet_info_ptr;

struct _assign_info;
typedef struct _assign_info assign_info;
typedef boost::shared_ptr<assign_info> assign_info_ptr;

struct _schedule_entry;
typedef struct _schedule_entry schedule_entry;
typedef boost::shared_ptr<schedule_entry> schedule_entry_ptr;

#include <exceptions.h>
#include <topology.h>
#include <reservations.h>

#endif // _MAPPERS_H
