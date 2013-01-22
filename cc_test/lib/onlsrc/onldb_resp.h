#ifndef _ONLDB_RESP_H
#define _ONLDB_RESP_H

namespace onl
{
  class onldb_resp
  {
    private:
      // <0 means there was an error, 0 is typically the negative respone, 1 the positive response
      int r;

      // result<0 this contains the error msg, 0/1 the reason why negative/positive
      std::string m;

    public:
      onldb_resp(int res, std::string msg) throw();
      ~onldb_resp() throw();

      int result() throw();
      std::string msg() throw();
  };

  class node_info
  {
    private:
      std::string node_;
      std::string state_;
      bool has_cp_;
      bool do_keeboot_;
      std::string cp_;
      unsigned short cp_port_;
      std::string type_;
      bool is_dependent_;

    public:
      node_info() throw();
      node_info(std::string node, std::string state, bool has_cp, bool do_keebot, std::string cp, unsigned short cp_port, std::string type, bool is_dependent) throw();
      node_info(const node_info& ni) throw();
      ~node_info() throw();
      
      node_info& operator=(const node_info& ni);

      std::string node() throw();
      std::string state() throw();
      bool has_cp() throw();
      bool do_keeboot() throw();
      std::string cp() throw();
      unsigned short cp_port() throw();
      std::string type() throw();
      bool is_dependent() throw();
  };

  typedef std::list<node_info> node_info_list;

  class switch_port_info
  {
    private:
      std::string switch_name;
      unsigned int port;
      bool interswitch_port;

    public:
      switch_port_info() throw();
      switch_port_info(std::string sw, unsigned int portnum, bool is_interswitch_port) throw();
      switch_port_info(const switch_port_info& spi) throw();
      ~switch_port_info() throw();

      switch_port_info& operator=(const switch_port_info& spi);
   
      std::string get_switch() { return switch_name; }
      unsigned int get_port() { return port; }
      bool is_interswitch_port() { return interswitch_port; }
  };

  class switch_info
  {
    private:
      std::string switch_name;
      unsigned int num_ports;
      unsigned int mgmt_port;

    public:
      switch_info() throw();
      switch_info(std::string sw, unsigned int numports, unsigned int mgmtport) throw();
      switch_info(const switch_info& si) throw();
      ~switch_info() throw();

      switch_info& operator=(const switch_info& si);

      std::string get_switch() { return switch_name; }
      unsigned int get_num_ports() { return num_ports; }
      void set_num_ports(unsigned int numports) { num_ports = numports; }
      unsigned int get_mgmt_port() { return mgmt_port; }
      void set_mgmt_port(unsigned int mgmtport) { mgmt_port = mgmtport; }
  };

  typedef std::list<switch_info> switch_info_list;
};

#endif // _ONLDB_RESP_H
