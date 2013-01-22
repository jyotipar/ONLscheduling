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
#include <fstream>
#include <list>
#include <vector>
#include <algorithm>

#include <time.h>
#include <sys/time.h>

#include "stats.h"
#include "mappers.h"

using namespace std;

const int BIGINT = 0x7fffffff;
inline double randfrac() { return ((double) random())/BIGINT; }
inline int randint(int lo, int hi) { return lo + (random() % (hi + 1 - lo)); }

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

std::string time_unix2str(time_t unix_time) throw()
{
  struct tm *stm = localtime(&unix_time);

  char char_str[16];
  sprintf(char_str,"%04d%02d%02d%02d%02d%02d",stm->tm_year+1900,stm->tm_mon+1,stm->tm_mday,stm->tm_hour,stm->tm_min,stm->tm_sec);
  std::string str = char_str;
  return str;
}

typedef struct _request
{
  std::string file_name;
  topology *t;
  time_t start1;
  time_t start2;
  unsigned int len;
  bool accepted;
  int host_cost;
  int intercluster_cost;
} request;

void
print_request_metric(request* req, const char* msg)
{
  cout << msg << ":user graph(" << req->file_name << ")" <<  std::endl;
  cout << "MAPPING: user graph(" << req->file_name << ") start:(" << req->start1 << ", " << req->start2 << ") duration:" << req->len << std::endl;
  req->t->print_resources();
  cout << "  METRIC: user graph(" << req->file_name << ") InterclusterCost(" << req->intercluster_cost << ") HostBW(" << req->host_cost << ")" << std::endl << std::endl;
}


int main(int argc, char **argv)
{
  if(argc != 6)
  {
    cout << "usage: request TESTBED_TOP USER_TOP_BASE START_TIMES ORDER NUM_USER_TOPS" << std::endl;
    return -1;
  }

  std::string testbed_topology = argv[1];
  std::string user_topology_base = argv[2];
  std::string times_file = argv[3];
  std::string order = argv[4];



  unsigned int num_user_tops = str2int(argv[5]);

  reservations *r;
  stats *stats_accept, *stats_reject, *stats_reject_quick;
  
  struct timeval start, stop;
  int req_res;

  srandom(2);

  cout << "params: " << testbed_topology << "," << user_topology_base << "," << times_file << "," << order  << "," << num_user_tops << std::endl;
  
  std::vector<request *> requests;
  int host_bw = 0;
  int ic_cost = 0;

  try
  {
    stats_accept = new stats(num_user_tops);
    stats_reject = new stats(num_user_tops);
    stats_reject_quick = new stats(num_user_tops);



    r = new reservations(testbed_topology);

    fstream fs(times_file.c_str(), fstream::in);
    if(fs.fail()) throw reservation_exception("file not found!");

    for(unsigned int i=1; i<=num_user_tops; ++i)
    {
      request *nr = new request();

      std::string fn = user_topology_base + "/" + to_string(i);
      nr->t = new topology(fn);
      nr->file_name = fn;
      std::string line;
      getline(fs, line);
      size_t found1, found2, found3;
      found1 = line.find(',',0);
      nr->start1 = str2int(line.substr(0,found1));
      found2 = line.find(',',found1+1);
      nr->start2 = str2int(line.substr(found1+1,found2));
      found3 = line.find(',',found2+1);
      nr->len = str2int(line.substr(found2+1,found3));

      nr->accepted = false;

      nr->host_cost = r->compute_host_cost(nr->t);
      nr->intercluster_cost = 0;
      requests.push_back(nr);

      //cout << to_string(nr->start1) << "," << to_string(nr->start2) << "," << to_string(nr->len) << std::endl;
    }

    if(order == "random")
    {
      random_shuffle(requests.begin(), requests.end());
    }
    else if(order == "reverse")
    {
      reverse(requests.begin(), requests.end());
    }
    
    time_t cur_time = time(NULL) + 60*60;

    std::vector<request *>::iterator req;
    for(req = requests.begin(); req != requests.end(); ++req)
    {
      std::string begin1 = time_unix2str(cur_time + (*req)->start1);
      std::string begin2 = time_unix2str(cur_time + (*req)->start2);
      unsigned int len = ((*req)->len)/60;

      if(gettimeofday(&start, NULL) < 0) { cout << "gettimeofday error" << endl; exit(0); }
      req_res = r->make_reservation(begin1, begin2, len, (*req)->t);
      if(gettimeofday(&stop, NULL) < 0) { cout << "gettimeofday error" << endl; exit(0); }

      unsigned int ms = 0;
      if(stop.tv_sec == start.tv_sec)
      {
        ms = (stop.tv_usec - start.tv_usec)/1000;
      }
      else
      {
        ms = ((1000000 - start.tv_usec)/1000) + (stop.tv_sec - start.tv_sec - 1)*1000 + (stop.tv_usec)/1000;
      }
      
      if(req_res == 2)
      {
        (*req)->accepted = true;
	(*req)->intercluster_cost = r->compute_intercluster_cost((*req)->t);
        stats_accept->add_sample(ms);
	print_request_metric((*req), "ACCEPTED");
	host_bw += (*req)->host_cost;
	ic_cost += (*req)->intercluster_cost;
      }
      else if(req_res == 1)
      {
        (*req)->accepted = false;
        stats_reject_quick->add_sample(ms);
	print_request_metric((*req), "REJECTED_QUICK");
        delete (*req)->t;
      }
      else //req_res == 0
      {
        (*req)->accepted = false;
        stats_reject->add_sample(ms);
	print_request_metric((*req), "REJECTED");
        delete (*req)->t;
      }
    }
  }
  catch(reservation_exception& re)
  {
    cout << "error: " << re.what() << std::endl;
    return -1;
  }

  cout << "TOTAL METRIC: InterclusterCost(" << ic_cost << ") HostBW(" << host_bw << ")" << std::endl << std::endl;

  stats_accept->calculate();
  stats_reject->calculate();
  stats_reject_quick->calculate();

  cout << "accept stats: ";
  stats_accept->print_summary();

  cout << "reject stats: ";
  stats_reject->print_summary();

  cout << "reject_quick stats: ";
  stats_reject_quick->print_summary();

  stats_accept->write_to_file(testbed_topology + "." + user_topology_base + "." + times_file + "." + order + ".accept");
  stats_reject->write_to_file(testbed_topology + "." + user_topology_base + "." + times_file + "." + order + ".reject");
  stats_reject_quick->write_to_file(testbed_topology + "." + user_topology_base + "." + times_file + "." + order + ".reject_quick");

  requests.clear();
  if(r) delete r;
  if(stats_accept) delete stats_accept;
  if(stats_reject) delete stats_reject;
  if(stats_reject_quick) delete stats_reject_quick;
}
