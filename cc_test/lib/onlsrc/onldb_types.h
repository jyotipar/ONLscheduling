#ifndef _ONLDB_TYPES_H
#define _ONLDB_TYPES_H

namespace onl
{
  // the firs set of these define the classes for each database table fully
  sql_create_16(users,1,16,
    mysqlpp::sql_varchar, user,
    mysqlpp::sql_smallint_unsigned, uid,
    mysqlpp::sql_varchar, password,
    mysqlpp::sql_tinyint_unsigned, priv,
    mysqlpp::sql_smallint_unsigned, score,
    mysqlpp::sql_varchar, respadmin,
    mysqlpp::sql_varchar, timezone,
    mysqlpp::sql_varchar, fullname,
    mysqlpp::sql_varchar, email,
    mysqlpp::sql_varchar, position,
    mysqlpp::sql_varchar, institution,
    mysqlpp::sql_varchar, phone,
    mysqlpp::sql_varchar, homepage,
    mysqlpp::sql_varchar, advisor,
    mysqlpp::sql_varchar, advisoremail,
    mysqlpp::sql_varchar, description)

  sql_create_3(groups,1,3,
    mysqlpp::sql_varchar, grp,
    mysqlpp::sql_smallint_unsigned, gid,
    mysqlpp::sql_varchar, respuser)

  sql_create_3(members,2,3,
    mysqlpp::sql_varchar, user,
    mysqlpp::sql_varchar, grp,
    mysqlpp::sql_tinyint_unsigned, prime)

  sql_create_4(observers,2,4,
    mysqlpp::sql_varchar, observer,
    mysqlpp::sql_varchar, observee,
    mysqlpp::sql_datetime, begin,
    mysqlpp::sql_datetime, end)

  sql_create_5(types,1,5,
    mysqlpp::sql_varchar, tid,
    mysqlpp::sql_varchar, type,
    mysqlpp::sql_tinyint_unsigned, daemon,
    mysqlpp::sql_tinyint_unsigned, keeboot,
    mysqlpp::sql_varchar, shortdesc)

  sql_create_4(interfacetypes,1,4,
    mysqlpp::sql_varchar, interface,
    mysqlpp::sql_varchar, description,
    mysqlpp::sql_varchar, type,
    mysqlpp::sql_smallint_unsigned, bandwidth)

  sql_create_7(nodes,1,7,
    mysqlpp::sql_varchar, node,
    mysqlpp::sql_varchar, tid,
    mysqlpp::sql_varchar, daemonhost,
    mysqlpp::sql_smallint_unsigned, daemonport,
    mysqlpp::sql_tinyint_unsigned, priority,
    mysqlpp::sql_varchar, acl,
    mysqlpp::sql_varchar, state)

  sql_create_3(interfaces,1,3,
    mysqlpp::sql_varchar, tid,
    mysqlpp::sql_smallint_unsigned, port,
    mysqlpp::sql_varchar, interface)

  sql_create_3(clustercomps,2,3,
    mysqlpp::sql_smallint_unsigned, compid,
    mysqlpp::sql_varchar, tid,
    mysqlpp::sql_varchar, comptype)

  sql_create_6(connections,1,6,
    mysqlpp::sql_smallint_unsigned, cid,
    mysqlpp::sql_smallint_unsigned, capacity,
    mysqlpp::sql_varchar, node1,
    mysqlpp::sql_smallint_unsigned, node1port,
    mysqlpp::sql_varchar, node2,
    mysqlpp::sql_smallint_unsigned, node2port)

  sql_create_5(hwclusters,1,5,
    mysqlpp::sql_varchar, cluster,
    mysqlpp::sql_varchar, tid,
    mysqlpp::sql_tinyint_unsigned, priority,
    mysqlpp::sql_varchar, acl,
    mysqlpp::sql_varchar, state)

  sql_create_3(hwclustercomps,1,3,
    mysqlpp::sql_varchar, node,
    mysqlpp::sql_varchar, cluster,
    mysqlpp::sql_tinyint_unsigned, dependent)

  sql_create_4(log,1,4,
    mysqlpp::sql_varchar, node,
    mysqlpp::sql_date, dt,
    mysqlpp::sql_varchar, category,
    mysqlpp::sql_varchar, notes)

  sql_create_5(reservations,1,5,
    mysqlpp::sql_mediumint_unsigned, rid,
    mysqlpp::sql_varchar, user,
    mysqlpp::sql_datetime, begin,
    mysqlpp::sql_datetime, end,
    mysqlpp::sql_varchar, state)

  sql_create_4(experiments,1,4,
    mysqlpp::sql_smallint_unsigned, eid,
    mysqlpp::sql_datetime, begin,
    mysqlpp::sql_datetime, end,
    mysqlpp::sql_mediumint_unsigned, rid)

  sql_create_3(nodeschedule,2,3,
    mysqlpp::sql_varchar, node,
    mysqlpp::sql_mediumint_unsigned, rid,
    mysqlpp::sql_tinyint_unsigned, fixed)

  sql_create_4(connschedule,3,4,
    mysqlpp::sql_smallint_unsigned, linkid,
    mysqlpp::sql_mediumint_unsigned, rid,
    mysqlpp::sql_smallint_unsigned, cid,
    mysqlpp::sql_smallint_unsigned, capacity)

  sql_create_3(hwclusterschedule,2,3,
    mysqlpp::sql_varchar, cluster,
    mysqlpp::sql_mediumint_unsigned, rid,
    mysqlpp::sql_tinyint_unsigned, fixed)

  sql_create_4(vswitchschedule,3,4,
    mysqlpp::sql_smallint_unsigned, vlanid,
    mysqlpp::sql_mediumint_unsigned, rid,
    mysqlpp::sql_smallint_unsigned, port,
    mysqlpp::sql_smallint_unsigned, linkid)

  sql_create_3(policy,1,3,
    mysqlpp::sql_varchar, parameter,
    mysqlpp::sql_smallint_unsigned, value,
    mysqlpp::sql_varchar, description)

  sql_create_9(typepolicy,4,9,
    mysqlpp::sql_varchar, tid,
    mysqlpp::sql_varchar, grp,
    mysqlpp::sql_datetime, begin,
    mysqlpp::sql_datetime, end,
    mysqlpp::sql_smallint_unsigned, maxlen,
    mysqlpp::sql_smallint_unsigned, usermaxnum,
    mysqlpp::sql_smallint_unsigned, usermaxusage,
    mysqlpp::sql_smallint_unsigned, grpmaxnum,
    mysqlpp::sql_smallint_unsigned, grpmaxusage)

  // the next set define classes that only partially reflect some tables for ease of updating
  sql_create_1(nodestates,1,0,
    mysqlpp::sql_varchar, state)

  sql_create_1(clusterstates,1,0,
    mysqlpp::sql_varchar, state)

  sql_create_1(nodenames,1,0,
    mysqlpp::sql_varchar, node)

  sql_create_1(clusternames,1,0,
    mysqlpp::sql_varchar, cluster)

  sql_create_1(passwords,1,0,
    mysqlpp::sql_varchar, password)

  sql_create_1(timezones,1,0,
    mysqlpp::sql_varchar, timezone)

  sql_create_1(policyvals,1,0,
    mysqlpp::sql_smallint_unsigned, value)

  sql_create_5(typepolicyvals,5,0,
    mysqlpp::sql_smallint_unsigned, maxlen,
    mysqlpp::sql_smallint_unsigned, usermaxnum,
    mysqlpp::sql_smallint_unsigned, usermaxusage,
    mysqlpp::sql_smallint_unsigned, grpmaxnum,
    mysqlpp::sql_smallint_unsigned, grpmaxusage)

  sql_create_2(clusterelems,2,0,
    mysqlpp::sql_smallint_unsigned, compid,
    mysqlpp::sql_varchar, type)

  sql_create_4(clusterconnelems,4,0,
    mysqlpp::sql_smallint_unsigned, comp1id,
    mysqlpp::sql_smallint_unsigned, comp1port,
    mysqlpp::sql_smallint_unsigned, comp2id,
    mysqlpp::sql_smallint_unsigned, comp2port)

  sql_create_3(otherrestimes,3,0,
    mysqlpp::sql_varchar, user,
    mysqlpp::sql_datetime, begin,
    mysqlpp::sql_datetime, end)

  sql_create_2(restimes,2,0,
    mysqlpp::sql_datetime, begin,
    mysqlpp::sql_datetime, end)

  sql_create_8(nodeinfo,8,0,
    mysqlpp::sql_varchar, node,
    mysqlpp::sql_varchar, state,
    mysqlpp::sql_tinyint_unsigned,  daemon,
    mysqlpp::sql_tinyint_unsigned,  keeboot,
    mysqlpp::sql_varchar, daemonhost,
    mysqlpp::sql_smallint_unsigned, daemonport,
    mysqlpp::sql_varchar, type,
    mysqlpp::Null<mysqlpp::sql_tinyint_unsigned>, dependent)

  sql_create_3(clusterinfo,3,0,
    mysqlpp::sql_varchar, cluster,
    mysqlpp::sql_varchar, state,
    mysqlpp::sql_varchar, type)

  sql_create_1(linkconn,1,0,
    mysqlpp::sql_smallint_unsigned, cid)

  sql_create_4(conninfo,4,0,
    mysqlpp::sql_varchar, node1,
    mysqlpp::sql_smallint_unsigned, node1port,
    mysqlpp::sql_varchar, node2,
    mysqlpp::sql_smallint_unsigned, node2port)

  sql_create_1(typetype,1,0,
    mysqlpp::sql_varchar, type)
  
  sql_create_1(lockresult,1,0,
    mysqlpp::Null<mysqlpp::sql_tinyint_unsigned>, lockres)
  
  sql_create_3(curresinfo,3,0,
    mysqlpp::sql_datetime, begin,
    mysqlpp::sql_datetime, end,
    mysqlpp::sql_mediumint_unsigned, rid)

  sql_create_2(resinfo,2,0,
    mysqlpp::sql_mediumint_unsigned, rid,
    mysqlpp::sql_varchar, state)

  sql_create_3(experimentins,3,0,
    mysqlpp::sql_datetime, begin,
    mysqlpp::sql_datetime, end,
    mysqlpp::sql_mediumint_unsigned, rid)

  sql_create_4(reservationins,4,0,
    mysqlpp::sql_varchar, user,
    mysqlpp::sql_datetime, begin,
    mysqlpp::sql_datetime, end,
    mysqlpp::sql_varchar, state)

  sql_create_2(userres,2,0,
    mysqlpp::sql_mediumint_unsigned, rid,
    mysqlpp::sql_varchar, user)

  sql_create_1(typenameinfo,1,0,
    mysqlpp::sql_varchar, tid)

  sql_create_1(privileges,1,0,
    mysqlpp::sql_tinyint_unsigned, priv)
  
  sql_create_1(resend,1,0,
    mysqlpp::sql_datetime, end)

  sql_create_4(hwclustertypes,4,0,
    mysqlpp::sql_varchar, cluster,
    mysqlpp::sql_varchar, tid,
    mysqlpp::sql_varchar, acl,
    mysqlpp::sql_tinyint_unsigned, fixed)

  sql_create_6(nodetypes,6,0,
    mysqlpp::sql_varchar, node,
    mysqlpp::sql_varchar, tid,
    mysqlpp::Null<mysqlpp::sql_varchar>, cluster,
    mysqlpp::sql_varchar, acl,
    mysqlpp::sql_varchar, daemonhost,
    mysqlpp::sql_tinyint_unsigned, fixed)
  
  sql_create_3(oldnodes,3,0,
    mysqlpp::sql_varchar, node,
    mysqlpp::sql_varchar, daemonhost,
    mysqlpp::sql_varchar, acl)
  
  sql_create_2(oldclusters,2,0,
    mysqlpp::sql_varchar, cluster,
    mysqlpp::sql_varchar, acl)

  sql_create_7(linkinfo,7,0,
    mysqlpp::sql_smallint_unsigned, linkid,
    mysqlpp::sql_smallint_unsigned, capacity,
    mysqlpp::sql_smallint_unsigned, cid,
    mysqlpp::sql_varchar, node1,
    mysqlpp::sql_smallint_unsigned, node1port,
    mysqlpp::sql_varchar, node2,
    mysqlpp::sql_smallint_unsigned, node2port)

  sql_create_3(baseclusterinfo,3,0,
    mysqlpp::sql_varchar, cluster,
    mysqlpp::sql_tinyint_unsigned, priority,
    mysqlpp::sql_varchar, tid)

  sql_create_2(specialclusterinfo,2,0,
    mysqlpp::sql_varchar, cluster,
    mysqlpp::sql_varchar, tid)

  sql_create_1(clustercluster,1,0,
    mysqlpp::sql_varchar, cluster)

  sql_create_1(switchports,1,0,
    mysqlpp::sql_smallint_unsigned, numports)

  sql_create_1(mgmtport,1,0,
    mysqlpp::sql_smallint_unsigned, port)

  sql_create_4(basenodeinfo,4,0,
    mysqlpp::sql_varchar, node,
    mysqlpp::sql_tinyint_unsigned, priority,
    mysqlpp::sql_varchar, tid,
    mysqlpp::Null<mysqlpp::sql_varchar>, cluster)

  sql_create_3(specialnodeinfo,3,0,
    mysqlpp::sql_varchar, node,
    mysqlpp::sql_varchar, tid,
    mysqlpp::Null<mysqlpp::sql_varchar>, cluster)

  sql_create_2(specnodeinfo,2,0,
    mysqlpp::sql_varchar, node,
    mysqlpp::sql_varchar, tid)

  sql_create_1(connid,1,0,
    mysqlpp::sql_smallint_unsigned, cid);

  sql_create_1(usernames,1,0,
    mysqlpp::sql_varchar, user);

  sql_create_1(capinfo,1,0,
    mysqlpp::sql_smallint_unsigned, capacity);

  sql_create_2(capconninfo,2,0,
    mysqlpp::sql_smallint_unsigned, cid,
    mysqlpp::sql_smallint_unsigned, capacity);

  sql_create_1(bwinfo,1,0,
    mysqlpp::sql_smallint_unsigned, bandwidth);

  sql_create_1(node2info,1,0,
    mysqlpp::sql_varchar, node2);

  sql_create_6(baselinkinfo,6,0,
    mysqlpp::sql_smallint_unsigned, cid,
    mysqlpp::sql_smallint_unsigned, capacity,
    mysqlpp::sql_varchar, node1,
    mysqlpp::sql_smallint_unsigned, node1port,
    mysqlpp::sql_varchar, node2,
    mysqlpp::sql_smallint_unsigned, node2port)

  sql_create_1(vswitches,1,0,
    mysqlpp::sql_smallint_unsigned, vlanid)

  sql_create_2(vswitchconns,2,0,
    mysqlpp::sql_smallint_unsigned, vlanid,
    mysqlpp::sql_smallint_unsigned, port)
};
#endif // _ONLDB_TYPES_H
