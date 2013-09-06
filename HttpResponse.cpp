// Copyright (c) 2013 Matt Hill
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.
//
// Handle the response from an HTTP Request.

#include "HttpResponse.h"

#include "HttpRequest.h"
#include "HttpException.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdarg>


HttpResponse::HttpResponse(const char *method, HttpRequest& request) :
  _state(StatusLine),
  _request(request),
  _method(method),
  _status(0),
  _version(0),
  _autoClose(false),  
  _bytesRead(0),
  _contentLength(-1),  
  _chunked(false),
  _chunkLength(0)
{
}


//-----------------------------------------------------------------------------
const char* HttpResponse::getHeader(const char *name) const
{
  std::string keyName(name);
  std::transform(keyName.begin(), keyName.end(), keyName.begin(), ::tolower);

  std::map<std::string, std::string>::const_iterator itr = _headers.find(keyName);

  if (itr == _headers.end())
  {
    return 0;
  }
  else
  {
    return itr->second.c_str();
  }
}


//-----------------------------------------------------------------------------
int HttpResponse::processResponse(const unsigned char *data, int sizeOfData)
{
  int byteCount = sizeOfData;

  std::string singleLine;

  while (byteCount > 0 && _state != Complete)
  {
    if (_state == StatusLine    ||
        _state == Header        ||
        _state == ChunkLength   ||
        _state == ChunkComplete ||
        _state == Trailer)        
    {
      // Loop until we find a newline.
      while (byteCount > 0)
      {
        char c = (char)*data++;
        byteCount--;

        if (c != '\n')
        {
          if (c != '\r')  //Ignore CR
            singleLine += c;
        }
        else //newline
        {
          switch (_state)
          {
            case StatusLine:
              processStatusLine(singleLine);
              break;

            case Header:
              processHeader(singleLine);
              break;

            case ChunkLength:
              processChunkLength(singleLine);
              break;
              
            case ChunkComplete:
              _state = ChunkLength;
              break;

            case Trailer:
              processTrailer(singleLine);
              break;

            default:
              break;
          }
          singleLine.clear();
          break;
        }
      }
    }
    else if (_state == Body)
    {
      int bytesProcessed = 0;

      if (_chunked)
      {
        bytesProcessed = processChunkedData(data, byteCount);
      }
      else
      {
        bytesProcessed = processData(data, byteCount);
      }

      data += bytesProcessed;
      byteCount -= bytesProcessed;
    }
  }

  return (sizeOfData - byteCount);
}


//-----------------------------------------------------------------------------
void HttpResponse::connectionClosed()
{
  if (_state == Complete)
    return;

  if (_state == Body && !_chunked && _contentLength == -1)
  {
    complete();
  }
  else
  {
    throw HttpException("Invalid State: in connectionClosed()");
  }
}


//-----------------------------------------------------------------------------
void HttpResponse::processStatusLine(std::string const &data)
{
  const char *c = data.c_str();

  while (*c && *c == ' ') { c++; }  //Skip spaces

  // Get Version
  while (*c && *c != ' ')
  {
    _versionStr += *c++;
  }

  while (*c && *c == ' ') { c++; }  //Skip spaces

  // Get Status Code
  std::string status;

  while (*c && *c != ' ')
  {
    status += *c++;
  }

  _status = atoi(status.c_str());

  while (*c && *c == ' ') { c++; }  //Skip spaces

  // Get Reason Phrase
  while (*c)
  {
    _reason += *c++;
  }

  if (_status < 100 || _status > 999)
  {
    throw HttpException("Invalid HTTP Status: (%s)", data.c_str());
  }

  if (_versionStr == "HTTP:/1.0")
  {
    _version = 10;
  }
  else if (0 == _versionStr.compare(0, 7, "HTTP/1."))
  {
    _version = 11;
  }
  else
  {
    throw HttpException("Invalid HTTP Version: (%s)", _versionStr.c_str());
  }
 
  // After processing the Status Line, move to the Header.
  _state = Header;
  _currHeader.clear();
}


//-----------------------------------------------------------------------------
void HttpResponse::processHeader(std::string const &data)
{
  // Done with Headers
  if (data.empty())
  {
    addHeader();

    // Ignore HTTP Status Code 100-Continue.
    if (_status == Continue)
    {
      _state = StatusLine;
    }
    else
    {
      initBody(); // Move on to the Body
    }

    return;
  }

  const char *c = data.c_str();

  // If data starts with whitespace, add to previous header.
  if (isspace(*c))
  {
    c++;

    while (*c && isspace(*c)) { c++; }  //Remove whitespace

    _currHeader += ' ';
    _currHeader += c;
  }
  else
  {
    addHeader();  //Add _currHeader to map and start a new _currHeader.
    _currHeader = c;
  }
}


//-----------------------------------------------------------------------------
void HttpResponse::processTrailer(std::string const &data)
{
  // Not processing trailing headers...
  complete();
}


//-----------------------------------------------------------------------------
int HttpResponse::processData(const unsigned char *data, int byteCount)
{
  int bytesProcessed = byteCount;

  if (_contentLength != -1)
  {
    int remaining = _contentLength - _bytesRead;

    if (bytesProcessed > remaining)
    {
      bytesProcessed = remaining;
    }
  }

  // Callback to pass data to the caller
  if (_request._receiveData)
  {
    (_request._receiveData)(this, _request._additionalParams, data, bytesProcessed);
  }

  _bytesRead += bytesProcessed;

  if (_contentLength != -1 && _bytesRead == _contentLength)
  {
    complete();
  }

  return bytesProcessed;
}


//-----------------------------------------------------------------------------
void HttpResponse::processChunkLength(std::string const &data)
{
  _chunkLength = strtol(data.c_str(), NULL, 16);  // Hex
  
  if (_chunkLength == 0)
  {
    // Done with Body, move to Trailer.
    _state = Trailer;
  }
  else
  {
    _state = Body;
  }
}


//-----------------------------------------------------------------------------
int HttpResponse::processChunkedData(const unsigned char *data, int byteCount)
{
  int bytesProcessed = byteCount;
  
  if (bytesProcessed > _chunkLength)
  {
    bytesProcessed = _chunkLength;
  }

  // Callback to pass data to the caller
  if (_request._receiveData)
  {
    (_request._receiveData)(this, _request._additionalParams, data, bytesProcessed);
  }

  _bytesRead += bytesProcessed;

  _chunkLength -= bytesProcessed;

  if (_chunkLength == 0)
  {
    _state = ChunkComplete;
  }

  return bytesProcessed;
}


//-----------------------------------------------------------------------------
// Is the server going to automatically close the connection?
bool HttpResponse::isAutoClose()
{
  // HTTP/1.x: Header "connection: close" if server connection will close connection.
  if (_version == 11)
  {
    const char *conn = getHeader("connection");

    if (conn && 0 == strcasecmp(conn, "close"))
    {
      return true;
    }
    else
    {
      return false;
    }
  }

  // HTTP/1.0: Header "keep-alive" if server will close connection.
  if (getHeader("keep-alive"))
  {
    return false;
  }

  return true;
}


//-----------------------------------------------------------------------------
void HttpResponse::addHeader()
{
  if (_currHeader.empty())
  {
    return;
  }

  const char *headerPair = _currHeader.c_str();

  std::string name;
  std::string value;

  // Read the header name (and ignore colon)
  while (*headerPair && *headerPair != ':')
  {
    name += tolower(*headerPair++);
  }

  if (*headerPair) headerPair++;  // Skip the colon

  // Skip Tabs and Spaces
  while (*headerPair && (*headerPair == '\t' || *headerPair == ' '))
  {
    headerPair++;
  }

  // Read the value
  value = headerPair;

  // Add name/value to Header map.
  _headers[name] = value;

  _currHeader.clear();
}


//-----------------------------------------------------------------------------
void HttpResponse::initBody()
{	
  _autoClose = isAutoClose();
  _contentLength = -1;

  const char *transferEncoding = getHeader("transfer-encoding");

  if (transferEncoding && 0 == strcasecmp(transferEncoding, "chunked"))
  {
    _chunked = true;
    _chunkLength = -1;
  }
  else
  {
    _chunked = false;
  }

  const char *length = getHeader("content-length");

  if (length && !_chunked)
  {
    _contentLength = atoi(length);
  }

  // These situations have no Body.
  if ((_status >= 100 && _status < 200) ||
      _status == NoContent              ||
      _status == NotModified            ||
      _method == "HEAD")
  {
    _contentLength = 0;
  }

  // If not chunked and no content-length, turn on _autoClose.
  if (!_autoClose && !_chunked && _contentLength == -1)
  {
    _autoClose = true;
  }

  // Callback to notify caller when Headers are ready
  if (_request._headersReady)
  {
    (_request._headersReady)(this, _request._additionalParams);
  }

  if (_chunked)
  {
    _state = ChunkLength;
  }
  else
  {
    _state = Body;
  }
}


//-----------------------------------------------------------------------------
void HttpResponse::complete()
{
  _state = Complete;

  // Callback to notify caller when response is complete
  if (_request._responseComplete)
  {
    (_request._responseComplete)(this, _request._additionalParams);
  }
}
