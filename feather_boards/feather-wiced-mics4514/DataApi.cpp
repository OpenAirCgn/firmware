/*
  DataApi.cpp - Library for sending data to an API.
  Created by Marcel Belledin, October 21, 2016.
*/

#include <Arduino.h>
#include <adafruit_feather.h>
#include <adafruit_featherap.h>
#include <adafruit_http.h>
#include "DataApi.h"

// Use the HTTP class
AdafruitHTTP http;

void receive_callback(void)
{
  // If there are incoming bytes available
  // from the server, read then print them:
  while ( http.available() )
  {
    int c = http.read();
    Serial.write( (isprint(c) || iscntrl(c)) ? ((char)c) : '.');
  }
}

void disconnect_callback(void)
{
  Serial.println("disconnected");
  http.stop();
}


DataApi::DataApi(const char* host, int port)
{
  _host = host;
  _port = port;
    // Setup the HTTP request with any required header entries
  http.addHeader("User-Agent", "curl/7.45.0");
  http.addHeader("Connection", "keep-alive");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.err_actions(true, false);

  // Set HTTP client verbose
  //http.verbose(true);

}


/*********************************************************************************************************
** Descriptions:           connect to known Access Point
*********************************************************************************************************/
bool DataApi::connect_ap(void)
{
  // Attempt to connect to an AP
  if ( Feather.connect() )
  {
    Serial.println("Connected!");
    send_flag = 1;
      // Tell the HTTP client to auto print error codes and halt on errors

    // Set the callback handlers
    http.setReceivedCallback(receive_callback);
    http.setDisconnectCallback(disconnect_callback);

    http.connect(_host, _port); // Will halt if an error occurs

    Serial.println("OK");

  }
  else
  {
    Serial.printf("Failed! %s (%d)", Feather.errstr(), Feather.errno());
    Serial.println();
  }
  Serial.println();

  return Feather.connected();
}

/*********************************************************************************************************
** Descriptions:           send to influx api
*********************************************************************************************************/
void DataApi::send_to_influx(char const* data, char const* url)
{
  if (send_flag) {
    Serial.println("dataAPI send");
    http.postRaw(url, data); // Will halt if an error occurs
  }
}






