/*
 * Copyright (c) 2008-2013 Charlie Wiseman, Jyoti Parwatikar and Washington University in St. Louis.
 * All rights reserved
 *
 * Distributed under the terms of the GNU General Public License v3
 * 
*/

#ifndef _STATS_H
#define _STATS_H

class stats
{
  private:
    int *samples;
    int max_num_samples; 
    int next_sample;
  
    unsigned int sum;
    double mean;
    double sdev;
    double serr;
    int max_sample;
    int min_sample;

  public:
    stats(int num_samples);
    ~stats();

    void add_sample(int sample);
    void calculate();
    void print_summary();
    void write_to_file(std::string fn);
};

#endif // _STATS_H
