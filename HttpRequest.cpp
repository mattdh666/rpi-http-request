// Copyright (c) 2013 Matt Hill
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#include "HttpRequest.h"

#include "HttpException.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <vector>

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>


//-----------------------------------------------------------------------------
// Convert IP or Hostname into a usable IP structure (in_addr).
struct in_addr* addressToIP(const char *address)
{
  // Case for IP address like: www.xxx.yyy.zzz 
  static struct in_addr addr;

  if (inet_aton(address, &addr) != 0)
  {
    return &addr;
  }

  // Case for hostname
  struct hostent *host;

  host = gethostbyname(address);

  if (host)
  {
    return (struct in_addr*)*host->h_addr_list;
  }

  return 0;
}


//-----------------------------------------------------------------------------
void socketError(const char *context)
{
  const char *msg = strerror(errno);
  throw HttpException("%s: %s", context, msg);
}


//-----------------------------------------------------------------------------
bool moreData(int socket)
{
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(socket, &fds);

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  int r = select(socket + 1, &fds, NULL, NULL, &tv);

  if (r < 0)
  {
    socketError("select()");
  }

  if (FD_ISSET(socket, &fds))
  {
    return true;
  }
  else
  {
    return false;
  }
}





//-----------------------------------------------------------------------------
HttpRequest::HttpRequest(const char *host, int port) :
  _headersReady(0),
  _receiveData(0),
  _responseComplete(0),
  _additionalParams(0),
  _state(Idle),
  _host(host),
  _port(port),
  _socket(-1)
{
}


//-----------------------------------------------------------------------------
HttpRequest::~HttpRequest()
{
  cleanUp();
}


//-----------------------------------------------------------------------------
void HttpRequest::initCallbacks(HeadersReady headersReady,
                                ReceiveData receiveData,
                                ResponseComplete responseComplete,
                                void *additionalParams)
{
  _headersReady = headersReady;
  _receiveData = receiveData;
  _responseComplete = responseComplete;
  _additionalParams = additionalParams;
}


//-----------------------------------------------------------------------------
void HttpRequest::sendRequest(const char *method,
                              const char *url,
                              const char *headers[],
                              const unsigned char *body,
                              int sizeOfBody)
{
  bool hasContentLength = false;

  // Check headers for content-length
  if (headers)
  {
    const char **itr = headers;

    while (*itr)
    {
      const char *name = *itr++;
      (void) *itr++;  //skip value

      if (0 == strcasecmp(name, "content-length"))
      {
        hasContentLength = true;
      }
    }
  }

  initRequest(method, url);

  if (body && !hasContentLength)
  {
    addHeader("Content-Length", sizeOfBody);
  }

  if (headers)
  {
    const char **itr = headers;

    while (*itr)
    {
      const char *name = *itr++;
      const char *value = *itr++;

      addHeader(name, value);
    }
  }

  sendHeaders();

  if (body)
  {
    send(body, sizeOfBody);
  }
}


//-----------------------------------------------------------------------------
void HttpRequest::processRequest()
{
  if (_pendingResponses.empty()) return;

  if (!moreData(_socket)) return;

  unsigned char data[MaxSocketRecvSize];
  
  int bytesReceived = recv(_socket, (char*)data, sizeof(data), 0);

  if (bytesReceived < 0)
  {
    socketError("recv()");
  }

  // No more data in the socket
  if (bytesReceived == 0)
  {
    HttpResponse *response = _pendingResponses.front();
    response->connectionClosed();
    delete response;
    _pendingResponses.pop_front();
    cleanUp();
  }
  // Process Response(s)
  else
  {
    int totalBytesProcessed = 0;

    while (totalBytesProcessed < bytesReceived && !_pendingResponses.empty())
    {
      HttpResponse *response = _pendingResponses.front();

      int bytesHandled = response->processResponse(&data[totalBytesProcessed], bytesReceived - totalBytesProcessed);

      if (response->completed())
      {
        delete response;
        _pendingResponses.pop_front();
      }

      totalBytesProcessed += bytesHandled;
    }
  }
}


//-----------------------------------------------------------------------------
void HttpRequest::cleanUp()
{
  if (_socket >= 0)
  {
    ::close(_socket);
  }

  _socket = -1;

  // Clear out any pending responses
  while (!_pendingResponses.empty())
  {
    delete _pendingResponses.front();
    _pendingResponses.pop_front();
  }
}




//-----------------------------------------------------------------------------
void HttpRequest::initSocket()
{
  in_addr *ip = addressToIP(_host.c_str());

  if (!ip)
  {
    throw HttpException("Invalid IP Address or Hostname.");
  }

  sockaddr_in address;
  memset((char*)&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(_port);
  address.sin_addr.s_addr = ip->s_addr;

  _socket = socket(AF_INET, SOCK_STREAM, 0);

  if (_socket < 0)
  {
    socketError("socket()");
  }

  if (::connect(_socket, (sockaddr const*)&address, sizeof(address)) < 0)
  {
    socketError("connect()");
  }
}


//-----------------------------------------------------------------------------
void HttpRequest::initRequest(const char *method, const char *url)
{
  if (_state != Idle)
  {
    throw HttpException("Request already started.");
  }

  _state = InProgress;

  char request[MaxRequestSize];
  sprintf(request, "%s %s HTTP/1.1", method, url);

  _currRequest.push_back(request);

  addHeader("Host", _host.c_str()); // For HTTP/1.1
  addHeader("Accept-Encoding", "identity");

  HttpResponse *response = new HttpResponse(method, *this);
  _pendingResponses.push_back(response);
}


//-----------------------------------------------------------------------------
void HttpRequest::addHeader(const char *name, const char *value)
{
  if (_state != InProgress)
  {
    throw HttpException("addHeader() failed");
  }

  _currRequest.push_back(std::string(name) + ": " + std::string(value));
}


//-----------------------------------------------------------------------------
void HttpRequest::addHeader(const char *name, int numericValue)
{
  char data[32];
  sprintf(data, "%d", numericValue);
  addHeader(name, data);
}


//-----------------------------------------------------------------------------
void HttpRequest::sendHeaders()
{
  assert(_state == InProgress);

  _state = Idle;
  _currRequest.push_back("");

  std::string msg;
  std::vector<std::string>::const_iterator itr;

  for(itr = _currRequest.begin(); itr != _currRequest.end(); itr++)
  {
    msg += (*itr) + "\r\n";
  }

  _currRequest.clear();

  send((const unsigned char*)msg.c_str(), msg.size());
}


//-----------------------------------------------------------------------------
void HttpRequest::send(const unsigned char *data, int sizeOfData)
{
  if (_socket < 0)
  {
    initSocket();
  }

  while (sizeOfData > 0)
  {
    int bytesSent = ::send(_socket, data, sizeOfData, 0);

    if (bytesSent < 0)
    {
      socketError("send()");
    }

    sizeOfData -= bytesSent;
    data += bytesSent;
  }
}

