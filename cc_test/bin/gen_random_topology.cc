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

int main(int argc, char **argv)
{
  topology *t = new topology();

  if(argc != 4)
  {
    cout << "usage: gen_random_topology BACKBONE_SIZE BACKBONE_EDGE_PROBABILITY SPAN" << std::endl;
    return -1;
  }

  unsigned int backbone_size;
  double backbone_edge_prob;
  unsigned int span;
  
  backbone_size = strtoul(argv[1],NULL,10);
  backbone_edge_prob = atof(argv[2]);
  span = strtoul(argv[3],NULL,10);

  std::string types[4] = {"A","B","C","D"};
  unsigned int num_ports[4] = {8,5,4,1};
  unsigned int num_available[4] = {4,14,6,82};
  unsigned int num_used[4] = {0,0,0,0};

  unsigned int *backbone_types = new unsigned int[backbone_size];
  unsigned int *next_port = new unsigned int[backbone_size];

  struct timeval now;
  gettimeofday(&now, NULL);
  srandom((now.tv_sec << 20) + now.tv_usec);

  int next_label = 1;
  try
  {
    for(unsigned int i=0; i<backbone_size; ++i)
    {
      int new_type = randint(0,2);
      while(num_used[new_type] >= num_available[new_type]) { new_type = randint(0,2); }
      num_used[new_type]++;
      backbone_types[i] = new_type;
      next_port[i] = 0;

      t->add_node(types[new_type], next_label);
      ++next_label;
    }

    for(unsigned int i=0; i<backbone_size; ++i)
    {
      unsigned int this_type = backbone_types[i];
      for(unsigned int port=next_port[i]; port<num_ports[this_type]; ++port)
      {
        if(i<(backbone_size-1) && (randfrac() <= backbone_edge_prob))
        {
          unsigned int j = randint(i+1,std::min(backbone_size-1, i+span));
          while(next_port[j] == num_ports[backbone_types[j]])
          {
            ++j;
            if(j == backbone_size) { break; }
          }
          if(j == backbone_size)
          {
            t->add_node(types[3],next_label);
            ++next_label;
            t->add_link(next_label, 1, i+1, port, next_label-1, 0);
            ++next_label;
          }
          else
          {
            t->add_link(next_label, 1, i+1, port, j+1, next_port[j]);
            ++next_port[j];
            ++next_label;
          }
        }
        else
        {
          t->add_node(types[3],next_label);
          ++next_label;
          t->add_link(next_label, 1, i+1, port, next_label-1, 0);
          ++next_label;
        }
      }
    }
  }
  catch(reservation_exception& re)
  {
    cout << "error: " << re.what() << std::endl;
  }

  t->write_to_stdout();

  if(t) delete t;
}
