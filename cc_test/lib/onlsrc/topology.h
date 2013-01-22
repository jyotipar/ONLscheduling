#ifndef _TOPOLOGY_H
#define _TOPOLOGY_H

namespace onl
{
  class topology
  {
    friend class onldb;

    private:
      std::list<node_resource_ptr> nodes;
      std::list<link_resource_ptr> links;

      std::string lowercase(std::string) throw();

    public:
      topology() throw();
      ~topology() throw();
   
      void print_resources() throw();

      // you have to add the hw resource before you add any links for it
      // you also have to add parents (clusters) before adding children (cluster components)
      onldb_resp add_node(std::string type, unsigned int label, unsigned int parent_label) throw();
      onldb_resp add_link(unsigned int label, unsigned int capacity, unsigned int node1_label, unsigned int node1_port, unsigned int node2_label, unsigned int node2_port) throw();

      onldb_resp remove_node(unsigned int label) throw();
      onldb_resp remove_link(unsigned int label) throw();

      std::string get_component(unsigned int label) throw();
      std::string get_type(unsigned int label) throw();
      unsigned int get_label(std::string node) throw();

      void get_conns(unsigned int label, std::list<int>& conn_list) throw();
  };
};
#endif // _TOPOLOGY_H
