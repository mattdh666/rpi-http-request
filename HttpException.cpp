// Copyright (c) 2013 Matt Hill
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#include "HttpException.h"

#include <cstdarg>
#include <cstdio>

HttpException::HttpException(const char* e, ...)
{
  va_list ap;
  va_start(ap, e);
  
  int n = vsnprintf(_message, MaxLength, e, ap);
  
  va_end(ap);
  
  if(n == MaxLength)
  {
    _message[MaxLength - 1] = '\0';
  }
}
