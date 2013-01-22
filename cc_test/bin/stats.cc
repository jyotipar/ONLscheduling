/*
 * Copyright (c) 2008-2013 Charlie Wiseman, Jyoti Parwatikar and Washington University in St. Louis.
 * All rights reserved
 *
 * Distributed under the terms of the GNU General Public License v3
 * 
*/

#include <math.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include "stats.h"

template <class T>
inline std::string to_str(const T& t)
{
  std::stringstream ss;
  ss << t;
  return ss.str();
}

stats::stats(int num_samples)
{
  samples = new int[num_samples];
  max_num_samples = num_samples;
  next_sample = 0;
  sum = 0;
}

stats::~stats()
{
  delete[] samples;
}

void stats::add_sample(int sample)
{
  if(next_sample == max_num_samples) { return; }

  if(next_sample == 0)
  {
    max_sample = sample;
    min_sample = sample;
  }

  if(sample > max_sample) { max_sample = sample; }
  if(sample < min_sample) { min_sample = sample; }

  sum += sample;

  samples[next_sample] = sample;
  ++next_sample;

  return;
}

void stats::calculate()
{
  if(next_sample == 0) return;

  mean = ((double)sum)/((double)next_sample);

  sdev = 0;
  for(int i=0; i<next_sample; ++i)
  {
    double diff = ((double)samples[i]) - mean;
    sdev += (diff*diff);
  }
  if(next_sample > 0)
  {
    sdev = (double)(sqrt(sdev/((double)next_sample)));
  }

  serr = 0;
  if(next_sample > 0)
  {
    serr = (double)(sdev/sqrt((double)next_sample));
  }

  return;
}

void stats::print_summary()
{
  std::cout << "n=" << to_str(next_sample);
  std::cout << " sum=" << to_str(sum);
  std::cout << " max=" << to_str(max_sample);
  std::cout << " min=" << to_str(min_sample);
  std::cout << " mean=" << to_str(mean);
  std::cout << " sdev=" << to_str(sdev);
  std::cout << " serr=" << to_str(serr);
  std::cout << std::endl;
  return;
}

void stats::write_to_file(std::string fn)
{
  std::fstream fs(fn.c_str(), std::fstream::out);

  fs << "n=" << to_str(next_sample);
  fs << " sum=" << to_str(sum);
  fs << " max=" << to_str(max_sample);
  fs << " min=" << to_str(min_sample);
  fs << " mean=" << to_str(mean);
  fs << " sdev=" << to_str(sdev);
  fs << " serr=" << to_str(serr);
  fs << std::endl;
  for(int i=0; i<next_sample; ++i)
  {
    fs << to_str(samples[i]) << std::endl;
  }

  fs.close();
  return;
}
