// Copyright (c) 2013 Matt Hill
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file. 
//
// This class, along with HttpResponse, manages an HTTP Connection.
// Several requests can be sent using a single instance of this class.
// The receiveData callback is used to process the Response payload.
//
// Basic Usage:
//
//   HttpRequest request("www.hyperceptive.org", 80);
//   request.initCallbacks(foo, bar, baz, 0);
//   request.sendRequest("GET", "/", 0, 0, 0);
//
//   while(request.responsesPending())
//   {
//     request.processRequest();
//   }
//

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "HttpResponse.h"

#include <deque>
#include <string>
#include <vector>


// Prototype for callbacks used to process an HTTP Response.
typedef void (*HeadersReady)(const HttpResponse *response, void *additionalParams);
typedef void (*ReceiveData)(const HttpResponse *response, void *additionalParams, const unsigned char *data, int sizeOfData);
typedef void (*ResponseComplete)(const HttpResponse *response, void *additionalParams);


class HttpRequest
{
  friend class HttpResponse;

public:

  static const int MaxRequestSize = 512;
  static const int MaxSocketRecvSize = 2048;


  HttpRequest(const char *host, int port);

  ~HttpRequest();

  // Initialize callbacks for handling the HTTP Response.
  // For each request, these are called by a corresponding HttpResponse object.
  //   headersReady     : Called when response Headers have been received.
  //   receiveData      : Called repeatedly to handle Body data.
  //   responseComplete : Called when response is complete.
  //   additionalParams : Passed back to all callbacks.
  void initCallbacks(HeadersReady headersReady,
                     ReceiveData receiveData,
                     ResponseComplete responseComplete,
                     void *additionalParams);

  // Make an HTTP request to the host and port specified in the Constructor.
  //   method     : GET, POST, HEAD, etc.
  //   url        : Path of URL, like "/fish/heads/yum.html"
  //   headers    : Array of name/value pairs terminated by NULL (0)
  //   body       : Body of request
  //   sizeOfBody : Size of the body
  void sendRequest(const char *method,
                   const char *url,
                   const char *headers[] = 0,
                   const unsigned char *body = 0,
                   int sizeOfBody = 0);

  bool responsesPending() const { return !_pendingResponses.empty(); }

  void processRequest();

  void cleanUp();


  //**************************************************************************
  // The following functions are used by sendRequest() to do the dirty work:
  //   - initRequest()
  //   - addHeader()
  //   - sendHeaders()
  //   - send()
  //
  // These functions can also be used directly by the caller.
  // Look at the implementation of the sendRequest() method for an example.
  //**************************************************************************

  void initSocket();

  // Initiate an HTTP Request
  //   method     : GET, POST, HEAD, etc.
  //   url        : Path of URL, like "/fish/heads/yum.html"  
  void initRequest(const char *method, const char *url);

  // Add a name/value pair to the request Header. Call after initRequest().
  void addHeader(const char *name, const char *value);  // value is char
  void addHeader(const char *name, int numericValue);   // value is int

  // Send the Headers over the socket. Call after adding all the Headers.
  void sendHeaders();

  // Send the data over the socket.
  void send(const unsigned char *data, int sizeOfData);


protected:

  HeadersReady     _headersReady;
  ReceiveData      _receiveData;
  ResponseComplete _responseComplete;

  void *_additionalParams;


private:

  enum { Idle, InProgress } _state;

  std::string _host;
  int _port;
  int _socket;

  std::vector<std::string> _currRequest;

  std::deque<HttpResponse*> _pendingResponses;

};

#endif 
