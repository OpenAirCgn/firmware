/*
  DataApi.h - Library for home SSID config.
  Created by Marcel Belledin, October 21, 2016.
*/
#ifndef DataApi_h
#define DataApi_h
#include <adafruit_feather.h>

class DataApi
{
  public:
    DataApi(const char* host,const int port);
    void send_to_influx(char const* data, const char* url);
    bool connect_ap(void);
    bool send_flag = 0;
  private:
    int _port;
    const char* _host;
};

#endif

