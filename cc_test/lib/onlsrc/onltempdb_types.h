#ifndef _ONLTEMPDB_TYPES_H
#define _ONLTEMPDB_TYPES_H

namespace onl
{
  sql_create_13(requests,1,13,
    mysqlpp::sql_varchar, user,
    mysqlpp::sql_varchar, password,
    mysqlpp::sql_varchar, respadmin,
    mysqlpp::sql_varchar, timezone,
    mysqlpp::sql_varchar, fullname,
    mysqlpp::sql_varchar, email,
    mysqlpp::sql_varchar, position,
    mysqlpp::sql_varchar, institution,
    mysqlpp::sql_varchar, phone,
    mysqlpp::sql_varchar, homepage,
    mysqlpp::sql_varchar, advisor,
    mysqlpp::sql_varchar, advisoremail,
    mysqlpp::sql_varchar, description)

  sql_create_14(rejections,2,14,
    mysqlpp::sql_varchar, user,
    mysqlpp::sql_smallint_unsigned, attempt,
    mysqlpp::sql_varchar, password,
    mysqlpp::sql_varchar, respadmin,
    mysqlpp::sql_varchar, timezone,
    mysqlpp::sql_varchar, fullname,
    mysqlpp::sql_varchar, email,
    mysqlpp::sql_varchar, position,
    mysqlpp::sql_varchar, institution,
    mysqlpp::sql_varchar, phone,
    mysqlpp::sql_varchar, homepage,
    mysqlpp::sql_varchar, advisor,
    mysqlpp::sql_varchar, advisoremail,
    mysqlpp::sql_varchar, description)
};
#endif // _ONLTEMPDB_TYPES_H
