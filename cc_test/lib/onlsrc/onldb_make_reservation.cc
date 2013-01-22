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
  current_time_unix = discretize_time(current_time_unix, divisor);
  current_time_db = time_unix2db(current_time_unix);

  begin1_unix = time_db2unix(begin1);
  begin1_unix = discretize_time(begin1_unix, divisor);
  begin1_db = time_unix2db(begin1_unix);

  begin2_unix = time_db2unix(begin2);
  begin2_unix = discretize_time(begin2_unix, divisor);
  begin2_db = time_unix2db(begin2_unix);

std::string JDD="jdd";

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
  
  if(begin1_unix > begin2_unix)
  {
    begin2_db = begin1_db;
    begin2_unix = begin1_unix;
  }
  
  if(begin1_unix < current_time_unix && begin2_unix < current_time_unix)
  {
    std::string errmsg;
    errmsg = "range of begin times from " + begin1_db + " to " + begin2_db + " is in the past";
    return onldb_resp(0,errmsg);
  }
  
  if(begin1_unix < current_time_unix)
  {
    begin1_db = current_time_db;
    begin1_unix = current_time_unix;
  }

  if(len <= 0)
  {
    std::string errmsg;
    errmsg = "length of " + to_string(len) + " minutes does not make sense";
    return onldb_resp(0,errmsg);
  }
 
  unsigned int chunk = 60/divisor;
  len = (((len-1) / chunk)+1) * chunk;

  if(t->nodes.size() == 0)
  {
    std::string errmsg;
    errmsg = "no components requested";
    return onldb_resp(0,errmsg);
  }

  // build a vector of type information for each type that is represented in the topology.
  for(hw = t->nodes.begin(); hw != t->nodes.end(); ++hw)
  {
    std::string type_type = get_type_type((*hw)->type);
    if(type_type == "")
    {
      std::string errmsg;
      errmsg = "type " + (*hw)->type + " not in the database";
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

  end1_unix = add_time(begin1_unix, len*60);
  end1_db = time_unix2db(end1_unix);

  end2_unix = add_time(begin2_unix, len*60);
  end2_db = time_unix2db(end2_unix);

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

  if(end1_unix > horizon_limit_unix)
  {
    std::string errmsg;
    errmsg = "requested times too far into the future (currently a " + to_string(horizon) + " day limit)";
    return onldb_resp(0,errmsg);
  }
 
  if(end2_unix > horizon_limit_unix)
  {
    end2_unix = horizon_limit_unix;
    end2_db = time_unix2db(end2_unix);
    begin2_unix = sub_time(end2_unix, len*60);
    begin2_db = time_unix2db(begin2_unix);
  }

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
  orig_time->b1_unix = begin1_unix;
  orig_time->b2_unix = begin2_unix;
  orig_time->e1_unix = end1_unix;
  orig_time->e2_unix = end2_unix;
  possible_times.push_back(orig_time);

  vector<time_range_ptr> new_possible_times;
  vector<time_range_ptr>::iterator tr;

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

    if((*ti)->num > tpv.usermaxnum)
    {
      std::string errmsg;
      errmsg = "you do not have access to that many " + (*ti)->type + "s (currently a " + to_string(tpv.usermaxnum) + " limit)";
      unlock("reservation");
      return onldb_resp(0,errmsg);
    }

    for(tr = possible_times.begin(); tr != possible_times.end(); ++tr)
    {
      time_t week_start = get_start_of_week((*tr)->b1_unix);
      time_t last_week_start = get_start_of_week((*tr)->e2_unix);
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

  if(possible_times.size() == 0)
  {
    unlock("reservation");
    return onldb_resp(0,(std::string)"your usage during that time period is already maxed out");
  }


  if (username == JDD) {
    cout << "Warning: Making reservation for JDD: testing for overlap" << endl;
  }
  // now go through each time frame and remove any time where the user already has a reservation
  for(tr = possible_times.begin(); tr != possible_times.end(); ++tr)
  {

    try
    {
      mysqlpp::Query query = onl->query();
      std::string b1_db = time_unix2db((*tr)->b1_unix);
      std::string e2_db = time_unix2db((*tr)->e2_unix);
      query << "select begin,end from reservations where user=" << mysqlpp::quote << username << " and state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << e2_db << " and end>" << mysqlpp::quote << b1_db << " order by begin";
      vector<restimes> rts;
      query.storein(rts);
  
      if(rts.empty())
      {
        if (username == JDD) {
          cout << "Warning: JDD: found no reservations for: b1_db: " << b1_db << "e2_db: " << e2_db << endl;
        }
        time_range_ptr new_time(new time_range());
        new_time->b1_unix = (*tr)->b1_unix;
        new_time->e1_unix = (*tr)->e1_unix;
        new_time->b2_unix = (*tr)->b2_unix;
        new_time->e2_unix = (*tr)->e2_unix;
        new_possible_times.push_back(new_time);
      }
      else
      {
        // each time in here overlaps the time range in tr
        bool add_left_over = false;
        vector<restimes>::iterator restime;
        cout << "Warning: JDD: found reservations for: b1_db: " << b1_db << "e2_db: " << e2_db << endl;
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
          if (username == JDD) {
            cout << "Warning: JDD: testing restime->begin: " << restime->begin << "restime->end: " << restime->end << endl;
            {
              std::string s = time_unix2db((*tr)->b1_unix);
              std::string e = time_unix2db((*tr)->e1_unix);
              cout << "Warning: JDD: testing (*tr)->b1_unix: " << s << "(*tr)->e1_unix: " << e << endl;
            }
            {
              std::string s = time_unix2db((*tr)->b2_unix);
              std::string e = time_unix2db((*tr)->e2_unix);
              cout << "Warning: JDD: testing (*tr)->b2_unix: " << s << "(*tr)->e2_unix: " << e << endl;
            }
          }
          if((*tr)->b1_unix < rb && rb >= (*tr)->e1_unix)
          {
            // JDD: if the reservation begin time is after both the begin and end time of the time frame (tr) being checked
            // JDD: then it is still a possible time.
            time_range_ptr new_time(new time_range());
            new_time->b1_unix = (*tr)->b1_unix;
            new_time->e1_unix = (*tr)->e1_unix;
            new_time->b2_unix = sub_time((*tr)->b2_unix, ((*tr)->e2_unix) - rb);
            new_time->e2_unix = rb;
            if (username == JDD) {
              cout << "Warning: JDD: calling new_possible_times.push_back()" << endl;
            }
            new_possible_times.push_back(new_time);
          }
          if((*tr)->e2_unix > re && re <= (*tr)->b2_unix)
          {
            (*tr)->e1_unix = add_time((*tr)->e1_unix, (re - (*tr)->b1_unix));
            (*tr)->b1_unix = re;
            if (username == JDD) {
              cout << "Warning: JDD: setting add_left_over = true" << endl;
              {
                std::string s = time_unix2db((*tr)->b1_unix);
                std::string e = time_unix2db((*tr)->e1_unix);
                cout << "Warning: JDD: NEW (*tr)->b1_unix: " << s << "(*tr)->e1_unix: " << e << endl;
              }
            }
            add_left_over = true;
          }
        }
        if(add_left_over)
        {
          time_range_ptr new_time(new time_range());
          new_time->b1_unix = (*tr)->b1_unix;
          new_time->e1_unix = (*tr)->e1_unix;
          new_time->b2_unix = (*tr)->b2_unix;
          new_time->e2_unix = (*tr)->e2_unix;
          if (username == JDD) {
            cout << "Warning: JDD: in if (add_left_over) calling new_possible_times.push_back() " << endl;
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
    
  if(possible_times.size() == 0)
  {
    unlock("reservation");
    return onldb_resp(0,(std::string)"you already have reservations during that time period");
  }

  // don't forget to check grpmaxnum from typepolicy..
  
  //std::cout << "current db time: " << current_time_db << std::endl << std::endl;
  for(tr = possible_times.begin(); tr != possible_times.end(); ++tr) 
  {
    //std::cout << "begin1 db time: " << time_unix2db((*tr)->b1_unix) << std::endl;
    //std::cout << "begin2 db time: " << time_unix2db((*tr)->b2_unix) << std::endl;
    //std::cout << "end1 db time: " << time_unix2db((*tr)->e1_unix) << std::endl;
    //std::cout << "end2 db time: " << time_unix2db((*tr)->e2_unix) << std::endl << std::endl;

    mysqlpp::Query query = onl->query();
    std::string b1_db = time_unix2db((*tr)->b1_unix);
    std::string e2_db = time_unix2db((*tr)->e2_unix);
    query << "select begin,end from reservations where state!='cancelled' and state!='timedout' and begin<" << mysqlpp::quote << e2_db << " and end>" << mysqlpp::quote << b1_db << " order by begin";
    vector<restimes> rts;
    query.storein(rts);

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
    while(toi_end != (*tr)->times_of_interest.end() && *toi_end < (*tr)->e1_unix) ++toi_end;
 
    time_t cur_start, cur_end;
    unsigned int increment = (60/divisor)*60;
    bool changed = true;
    for(cur_start = (*tr)->b1_unix, cur_end = (*tr)->e1_unix; cur_start <= (*tr)->b2_unix; cur_start = add_time(cur_start, increment), cur_end = add_time(cur_end, increment))
    {
      if (username == JDD) {
        // JDD
        std::string s = time_unix2db(cur_start);
        std::string e = time_unix2db(cur_end);
        cout << "Warning: JDD: testing cur_start: " << s << "cur_end: " << e << endl;
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

      if(toi_start != (*tr)->times_of_interest.end() && *toi_start < cur_start)
      {
        changed = true;
        ++toi_start;
      }
      if(toi_end != (*tr)->times_of_interest.end() && *toi_end < cur_end)
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

