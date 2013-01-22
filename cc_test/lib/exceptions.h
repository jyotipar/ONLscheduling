/*
 * Copyright (c) 2008-2013 Charlie Wiseman, Jyoti Parwatikar and Washington University in St. Louis.
 * All rights reserved
 *
 * Distributed under the terms of the GNU General Public License v3
 * */

#ifndef _EXCEPTIONS_H
#define _EXCEPTIONS_H

class reservation_exception: std::exception
{
  private:
    std::string info;

  public:
    reservation_exception(std::string s) throw()
    {
      info = s;
    }
    ~reservation_exception() throw()
    {
    }
    virtual const char* what() const throw()
    {
      return info.c_str();
    }
};

#endif // _EXCEPTIONS_H
