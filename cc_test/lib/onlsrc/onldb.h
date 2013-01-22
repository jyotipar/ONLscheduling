#ifndef _ONLDB_H
#define _ONLDB_H

namespace onl
{
  class onldb
  {
    private:
      mysqlpp::Connection *onl;

      bool lock(std::string l) throw();
      void unlock(std::string l) throw();

      // returned vals are based on times in current local TZ
      std::string time_unix2db(time_t unix_time) throw();
      int str2int(std::string s) throw();
      time_t time_db2unix(std::string db_time) throw();

      time_t get_start_of_week(time_t time) throw();
      time_t discretize_time(time_t time, unsigned int hour_divisor) throw();
      time_t add_time(time_t time, unsigned int seconds) throw();
      time_t sub_time(time_t time, unsigned int seconds) throw();

      std::string get_type_type(std::string type) throw();
      onldb_resp is_infrastructure(std::string node) throw();
      onldb_resp verify_clusters(topology *t) throw();

      onldb_resp handle_special_state(std::string state, std::string node, unsigned int len, bool extend) throw();
      onldb_resp clear_special_state(std::string state, std::string new_state, std::string node) throw();


      bool add_link(topology* t, int rid, unsigned int cur_link, unsigned int linkid, unsigned int cur_cap, unsigned int node1_label, unsigned int node1_port, unsigned int node2_label, unsigned int node2_port) throw();
      onldb_resp get_topology(topology *t, int rid) throw();
      void build_assign_list(node_resource_ptr hw, std::list<assign_info_ptr> *l) throw();
      onldb_resp fill_in_topology(topology *t, int rid) throw();
      bool subset_assign(std::list<assign_info_ptr> rl, std::list< std::list<assign_info_ptr>* >::iterator ali, std::list< std::list<assign_info_ptr>* >::iterator end, unsigned int level) throw();
      bool find_mapping(node_resource_ptr abs_node, node_resource_ptr res_node, unsigned int level) throw();


      onldb_resp get_base_topology(topology *t, std::string begin, std::string end) throw();    
      onldb_resp add_special_node(topology *t, std::string begin, std::string end, node_resource_ptr node) throw();
      //onldb_resp try_reservation(topology *t, std::string user, std::string begin, std::string end) throw();//JP changed 3/29/12
      onldb_resp try_reservation(topology *t, std::string user, std::string begin, std::string end, std::string state = "pending") throw();
      bool find_embedding(topology* req, topology* base, std::list<assign_info_ptr> al) throw(GRBException);
      bool embed(node_resource_ptr user, node_resource_ptr testbed) throw();
      //onldb_resp add_reservation(topology *t, std::string user, std::string begin, std::string end) throw();//JP changed 3/29/12
      onldb_resp add_reservation(topology *t, std::string user, std::string begin, std::string end, std::string state = "pending") throw();
      //bool has_reservation(std::string user, std::string begin, std::string end) throw();
 
      onldb_resp check_interswitch_bandwidth(topology* t, std::string begin, std::string end) throw();

    public:
      onldb() throw();
      ~onldb() throw();

      onldb_resp print_types() throw();

      onldb_resp clear_all_experiments() throw();
      onldb_resp get_switch_list(switch_info_list& list) throw();
      onldb_resp get_base_node_list(node_info_list& list) throw();
      onldb_resp get_node_info(std::string node, bool is_cluster, node_info& info) throw();

      onldb_resp get_state(std::string node, bool is_cluster) throw();
      onldb_resp set_state(std::string node, std::string state, unsigned int len=0) throw();
      onldb_resp put_in_testing(std::string node, unsigned int len=0) throw();
      onldb_resp remove_from_testing(std::string node) throw();
      onldb_resp extend_repair(std::string node, unsigned int len) throw();
      onldb_resp extend_testing(std::string node, unsigned int len) throw();

      onldb_resp get_type(std::string node, bool is_cluster) throw();
      onldb_resp get_node_from_cp(std::string cp) throw();

      onldb_resp authenticate_user(std::string username, std::string password_hash) throw();
      onldb_resp is_admin(std::string username) throw();

      // begin1,begin2 are strings in YYYYMMDDHHMMSS form, and should be in CST6CDT timezone
      // len is in minutes
      onldb_resp make_reservation(std::string username, std::string begin1, std::string begin2, unsigned int len, topology *t) throw();
      onldb_resp reserve_all(std::string begin, unsigned int len) throw();

      onldb_resp fix_component(topology *t, unsigned int label, std::string node) throw();
      onldb_resp cancel_current_reservation(std::string username) throw();
      onldb_resp extend_current_reservation(std::string username, int len) throw();
      onldb_resp has_reservation(std::string username) throw();

      onldb_resp assign_resources(std::string username, topology *t) throw();
      onldb_resp return_resources(std::string username, topology *t) throw();
      
      onldb_resp get_expired_sessions(std::list<std::string>& users) throw();
      onldb_resp get_capacity(std::string type, unsigned int port) throw();
      onldb_resp get_switch_ports(unsigned int cid, switch_port_info& info1, switch_port_info& info2) throw();
  };
};
#endif // _ONLDB_H
