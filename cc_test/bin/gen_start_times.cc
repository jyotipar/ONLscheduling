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
#include <math.h>

using namespace std;

const int BIGINT = 0x7fffffff;
inline double randfrac() { return ((double) random())/BIGINT; }
inline int randint(int lo, int hi) { return lo + (random() % (hi + 1 - lo)); }
inline double randexp(double mu) { return -mu*log(randfrac()); }

int main(int argc, char **argv)
{
  if(argc != 4)
  {
    cout << "usage: gen_start_times NRS LOAD FLEX" << std::endl;
    return -1;
  }

  unsigned int nrs; // normalized topology size
  double load; // desired average load
  unsigned int flex; // start time flexibility fraction of duration
  
  nrs = strtoul(argv[1],NULL,10);
  load = atof(argv[2]);
  flex = strtoul(argv[3],NULL,10);

  struct timeval now;
  gettimeofday(&now, NULL);
  srandom((now.tv_sec << 20) + now.tv_usec);

  unsigned int len = 60*60*2; // seconds

  double old_nrs = ((double)nrs)/((double)24.0);
  double tau = (old_nrs * len)/load;

  unsigned int start_time = 0;

  for(int i=1; i<=10000; ++i)
  {
    unsigned int start_time2 = start_time + (len * flex);
    cout << start_time << "," << start_time2 << "," << len << std::endl;
    start_time += randexp(tau);
  }
}
