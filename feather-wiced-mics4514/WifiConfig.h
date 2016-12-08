/*
  WifiConfig.h - Library for home SSID config.
  Created by Marcel Belledin, October 19, 2016.
*/
#ifndef WifiConfig_h
#define WifiConfig_h

class WifiConfig
{
  public:
    WifiConfig();
    void initialize(void);
    bool connect_with_ssid(void);
    void start_ap(void);
    char api_key[13]; // sprintf
    char board[7]; // sprintf overwrites
    bool connectable = 0;
  private:
    uint8_t mac[6];
};

#endif

