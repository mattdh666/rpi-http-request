// Copyright (c) 2013 Matt Hill
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#include "HttpRequest.h"
#include "HttpException.h"

#include <stdio.h>
#include <string.h>


//*********************************************
// Callbacks for processing the HTTP Response
//*********************************************

int payloadSize = 0;

void headersReady(const HttpResponse *response, void *additionalParams)
{
  printf("HTTP Status: %d - %s\n", response->getStatus(), response->getReason());
  printf(".................... Data Start ....................\n");
  payloadSize = 0;
}

void receiveData(const HttpResponse *response, void *additionalParams, const unsigned char *data, int sizeOfData)
{
  fwrite(data, 1, sizeOfData, stdout);
  payloadSize += sizeOfData;
}

void responseComplete(const HttpResponse *response, void *additionalParams)
{
  printf("\n.................... Data End ......................\n");
  printf("Data Size: %d bytes\n\n", payloadSize);
}



//*********************************************
// Demo Requests
//*********************************************

void demoGet()
{
  printf("\n-------------------------- GET Request --------------------------\n");
  HttpRequest request("hyperceptive.org", 80);
  request.initCallbacks(headersReady, receiveData, responseComplete, 0);
  request.sendRequest("GET", "/", 0, 0, 0);

  while(request.responsesPending())
  {
    request.processRequest();
  }
}


void demoPost()
{
  printf("\n-------------------------- POST Request -------------------------\n");
  const char *headers[] =
  {
    "Connection", "close",
    "Content-type", "application/x-www-form-urlencoded",
    "Accept", "text/plain",
    0  //null terminator
  };

  const char *body = "cdip_path=data_access%2Fjustdar.cdip%3F142%2Bdd%2B";

  HttpRequest request("codebones.com", 80);
  request.initCallbacks(headersReady, receiveData, responseComplete, 0);
  request.sendRequest("POST", "/cdipProxy.php", headers, (const unsigned char*)body, strlen(body));

  while(request.responsesPending())
  {
    request.processRequest();
  }
}


//-----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  try
  {
    demoGet();
    demoPost();
  }
  catch(HttpException &e)
  {
    printf("Exception:\n%s\n", e.message());
  }

  return 0;
}
