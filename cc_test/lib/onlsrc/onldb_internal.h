#ifndef _ONLDB_INTERNAL_H
#define _ONLDB_INTERNAL_H

namespace onl
{
  typedef struct _type_info
  {
    std::string type;
    std::string type_type;
    int num;
    int grpmaxnum;
  } type_info;
  
  typedef boost::shared_ptr<type_info> type_info_ptr;

  typedef struct _topology_resource
  {
    unsigned int label;
    topology cluster;
  } topology_resource;

  typedef boost::shared_ptr<topology_resource> topology_resource_ptr;

  typedef struct _time_range
  {
    time_t b1_unix;
    time_t b2_unix;
    time_t e1_unix;
    time_t e2_unix;
    std::list<time_t> times_of_interest;
  } time_range;
  
  typedef boost::shared_ptr<time_range> time_range_ptr;
};

#endif // _ONLDB_INTERNAL_H
