#ifndef _ONLTEMPDB_H
#define _ONLTEMPDB_H

namespace onl
{
  class onltempdb
  {
    private:
      mysqlpp::Connection *onltemp;

    public:
      onltempdb() throw();
      ~onltempdb() throw();
  };
};
#endif // _ONLTEMPDB_H
