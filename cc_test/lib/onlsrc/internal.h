#ifndef _INTERNAL_H
#define _INTERNAL_H

namespace onl
{
  template <class T>
  inline std::string to_string(const T& t)
  {
    std::stringstream ss;
    ss << t;
    return ss.str();
  }

  struct _link_resource;

  typedef struct _node_resource
  {
    // these are filled in at creation time
    std::string type;
    unsigned int label;
    bool is_parent;
    boost::shared_ptr<struct _node_resource> parent;
    std::list< boost::shared_ptr<struct _node_resource> > node_children;
    std::list< boost::shared_ptr<struct _link_resource> > links;

    // this will be filled in if the resource becomes part of an actual experiment
    bool fixed;
    std::string node; // initialized to ""
    std::string acl; // initialized to "unused"
    std::string cp; // initialized to "unused"

    // these are filled in later and are used for auxiliary purposes by the implementation
    std::string type_type; // initialized to ""
    bool marked; // initialized to false
    unsigned int level; // initialized to 0
    unsigned int priority;  //initialized to 0
    unsigned int in;  //initialized to 0
    unsigned int mip_id; // initialized to 0
  } node_resource;

  typedef boost::shared_ptr<node_resource> node_resource_ptr;

  typedef struct _link_resource
  {
    // these are filled in at creation time
    unsigned int label;
    unsigned int capacity;
    node_resource_ptr node1;
    unsigned int node1_port;
    node_resource_ptr node2;
    unsigned int node2_port;

    // these will be filled in if the resource becomes part of an actual experiment
    std::list<int> conns;

    // these are filled in later and are used for auxiliary purposes by the implementation
    bool marked; // initialized to false
    unsigned int level; // initialized to 0
    unsigned int in; // initialized to 0
    unsigned int linkid; // initialized to 0
  } link_resource;

  typedef boost::shared_ptr<link_resource> link_resource_ptr;

  typedef struct _assign_info
  {
    std::string type;
    std::string type_type;
    bool marked;
    std::list<node_resource_ptr> user_nodes;
    std::list<node_resource_ptr> testbed_nodes;
  } assign_info;

  typedef boost::shared_ptr<assign_info> assign_info_ptr;
};

#endif // _INTERNAL_H
