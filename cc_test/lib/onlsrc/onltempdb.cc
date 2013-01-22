#include <string>
#include <iostream>
#include <vector>

#include <mysql++/mysql++.h>
#include <mysql++/ssqls.h>
#include <boost/shared_ptr.hpp>

#include "internal.h"
#include "onldb_resp.h"
#include "onltempdb.h"
#include "onltempdb_types.h"

using namespace std;
using namespace onl;

#define ONLTEMPDB     "onltempnew"
#define ONLTEMPDBHOST "localhost"
#define ONLTEMPDBUSER "onltempadmin"
#define ONLTEMPDBPASS "onltemprocks!"


onltempdb::onltempdb() throw()
{
  onltemp = new mysqlpp::Connection(ONLTEMPDB,ONLTEMPDBHOST,ONLTEMPDBUSER,ONLTEMPDBPASS);
}

onltempdb::~onltempdb() throw()
{
  delete onltemp;
}
