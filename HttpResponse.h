// Copyright (c) 2013 Matt Hill
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file. 
//
// Handle the response from an HTTP Request.

#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <map>
#include <string>

class HttpRequest;

class HttpResponse
{
  friend class HttpRequest;

public:

  // HTTP Status Codes - only these are used explicitly
  static const int Continue    = 100;
  static const int NoContent   = 204;
  static const int NotModified = 304;

  // HTTP Status Code
  int getStatus() const { return _status; }

  // HTTP Reason Phrase
  const char* getReason() const { return _reason.c_str(); }

  // Get value of a header name/value pair. Return 0 if name doesn't exist.
  const char* getHeader(const char* name) const;

  bool completed() const { return (_state == Complete); }

  // Will the connection close when this response completes?
  bool autoClose() const { return _autoClose; }

protected:

  HttpResponse(const char *method, HttpRequest &request);

  // Process an HTTP Response or a Chunk of an HTTP Response.
  // Return the number of bytes used or 0 when complete.
  int processResponse(const unsigned char* data, int sizeOfData);

  void connectionClosed();

private:

  // Current state of this HTTP Response
  enum {
    StatusLine,    // Reading the Status Line
    Header,        // Reading Header lines
    Body,          // Reading the Body (or a Chunk)
    ChunkLength,   // Getting the length of a chunk
    ChunkComplete, // Done with a Chunk
    Trailer,       // Getting trailer after a Body
    Complete,      // Done with this Response
  } _state;

  HttpRequest& _request; // Corresponding HTTP Request

  std::string _method; // GET, POST, HEAD, etc.

  // Status Line
  int _status;  // Status Code
  std::string _reason; // Reason Phrase

  int _version; // 10: HTTP/1.0, 11: HTTP/1.x
  std::string _versionStr;

  // Header name/value pairs
  std::map<std::string, std::string> _headers;

  // For Header processing
  std::string _currHeader;

  // Command & Control
  bool _autoClose;     // Will the connection be closed after the response?
  int  _bytesRead;     // Bytes read from the Body
  int  _contentLength; // Content length
  bool _chunked;       // Chunked response?
  int  _chunkLength;   // Length of current chunk

  // Methods
  void processStatusLine(std::string const& data);
  void processHeader(std::string const& data);
  void processTrailer(std::string const& data);
  int  processData(const unsigned char* data, int byteCount);

  // Work with Chunks
  void processChunkLength(std::string const& data);
  int  processChunkedData(const unsigned char* data, int byteCount);

  // Helpers
  bool isAutoClose();
  void addHeader();
  void initBody();
  void complete();
};

#endif
