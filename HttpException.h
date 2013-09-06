// Copyright (c) 2013 Matt Hill
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file. 

#ifndef HTTP_EXCEPTION_H
#define HTTP_EXCEPTION_H

class HttpException
{
public:
  static const int MaxLength = 256;

  HttpException(const char* e, ...);
  
  const char* message() const { return _message; }

protected:  
  char _message[MaxLength];
};

#endif 


