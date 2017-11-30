/*
  WifiConfig.h - Library for home SSID config.
  Created by Marcel Belledin, October 19, 2016.
*/
#include <Arduino.h>
#include <adafruit_feather.h>
#include <adafruit_featherap.h>
#include <adafruit_http_server.h>

#include "WifiConfig.h"

#define WLAN_PASS            "openaircgn"
#define WLAN_ENCRYPTION       ENC_TYPE_WPA2_AES
#define WLAN_CHANNEL          1

#define PORT                  80                     // The TCP port to use.  23 is telnet
#define MAX_CLIENTS           3

IPAddress apIP     (192, 168, 1, 1);
IPAddress apGateway(192, 168, 0, 1);
IPAddress apNetmask(255, 255, 255, 0);

// Declare HTTPServer with max number of pages
AdafruitHTTPServer httpserver(4, WIFI_INTERFACE_SOFTAP);


const char hello_html[] = "<!DOCTYPE HTML>\r\n<html><h1>OpenAir Sensor Config</h1><form method='get' action='setting.html'><label>SSID (Name des Hotspots): </label><input name='ssid' length=32><label>Passwort: </label><input name='pass' length=64><input type='submit'></form>Hinweis: Wir ben&ouml;tigen den WLAN Zugang, um die Sensormessungen an unsere Datenbank zu senden. Nachdem das Ger&auml;t die Daten empfangen hat, testet es die Verbindung. Ihr Browser zeigt dabei eine Fehlerseite...Wenn die Daten nach wenigen Minuten unter <a href=\"http://openair.codingcologne.de\">openair.codingcologne.de</a> nicht erscheinen, melden sie sich. (siehe Anleitung)</html>";
const char not_found[] = "<!DOCTYPE HTML>\r\n<html>Die Seite existiert nicht</html>";

String escapeParameter(String param) {
  param.replace("+", " ");
  param.replace("%21", "!");
  param.replace("%23", "#");
  param.replace("%24", "$");
  param.replace("%26", "&");
  param.replace("%27", "'");
  param.replace("%28", "(");
  param.replace("%29", ")");
  param.replace("%2A", "*");
  param.replace("%2B", "+");
  param.replace("%2C", ",");
  param.replace("%2F", "/");
  param.replace("%3A", ":");
  param.replace("%3B", ";");
  param.replace("%3D", "=");
  param.replace("%3F", "?");
  param.replace("%40", "@");
  param.replace("%5B", "[");
  param.replace("%5D", "]");
  return param;
}
/*********************************************************************************************************
** Descriptions:     This callback is fired when user sends ssid and password
*********************************************************************************************************/
void save_setting(const char* url, const char* query, httppage_request_t* http_request)
{
  (void) url;
  (void) query;
  (void) http_request;
  String s=query;
  int firstIndex = s.indexOf("ssid=");
  int secondIndex = s.indexOf('&', firstIndex+1);
  int thirdIndex = s.indexOf("pass=", secondIndex+1);
  String ssid_ap = escapeParameter(s.substring(firstIndex+5, secondIndex));
  String pass_ap = escapeParameter(s.substring(thirdIndex+5)); // To the end of the string
  Serial.println("params:");
  Serial.println(ssid_ap);
  Serial.println(pass_ap);

  if ( Feather.connect(ssid_ap.c_str(),  pass_ap.c_str()) )
  {
    Serial.println("Connected!");
    httpserver.print("Erfolgreich verbunden. Bitte den Sensor wieder ein und ausstecken, damit er neu startet.");
  }
  else
  {
    Serial.printf("Failed! %s (%d)", Feather.errstr(), Feather.errno());
    Serial.println();
    httpserver.print("Es gab ein Fehler, bitte noch einmal versuchen.");
  }
  Serial.print("Saving current connected network to profile list ... ");
  if ( Feather.saveConnectedProfile() )
  {
    Serial.println("OK");
    if (Feather.connected()) {
      Feather.disconnect();
      //restart command
    }
  }else
  {
    Serial.println("Failed");
  }
}

HTTPPage pages[] =
{
  HTTPPageRedirect("/", "/hello.html"), // redirect root to hello page
  HTTPPage("/hello.html", HTTP_MIME_TEXT_HTML, hello_html),
  HTTPPage("/setting.html" , HTTP_MIME_TEXT_HTML, save_setting),
  HTTPPage("/404.html" , HTTP_MIME_TEXT_HTML, not_found)
};

uint8_t pagecount = sizeof(pages)/sizeof(HTTPPage);



/*********************************************************************************************************
** Descriptions:     This callback is fired when client disconnect from server
*********************************************************************************************************/
void client_disconnect_callback(void)
{
  httpserver.stop();
}



WifiConfig::WifiConfig()
{
 //todo: allow custom ssid/password over config
}


/*********************************************************************************************************
** Descriptions:            tries to connect to known ssid, or get new ssid
*********************************************************************************************************/
void WifiConfig::initialize(void)
{
  Feather.macAddress(mac);
  sprintf(api_key, "%02X%02X%02X%02X%02X%02X",  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  sprintf(board, "%02X%02X%02X", mac[3], mac[4], mac[5]);
  if (connect_with_ssid()) {
    // we have a known AP
    Serial.print("connectable");
    Feather.disconnect();
    connectable = 1;
  } else {
    // we need SSID credentials
    Serial.print("start AP");
    start_ap();
  }
}


/*********************************************************************************************************
** Descriptions:            start config
*********************************************************************************************************/
bool WifiConfig::connect_with_ssid(void)
{
 // Print AP profile list
  Serial.println("Saved AP profile");
  Serial.println("ID SSID                 Sec");
  for(uint8_t i=0; i<WIFI_MAX_PROFILE; i++)
  {
    char * profile_ssid = Feather.profileSSID(i);
    int32_t profile_enc = Feather.profileEncryptionType(i);

    Serial.printf("%02d ", i);
    if ( profile_ssid != NULL )
    {
      Serial.printf("%-20s ", profile_ssid);
      Feather.printEncryption( profile_enc );
      Serial.println();
    }else
    {
      Serial.println("Not Available ");
    }
    Feather.connect(); // no parameters --> using saved profiles

    return Feather.connected();
  }
}

/*********************************************************************************************************
** Descriptions:            start config
*********************************************************************************************************/
void WifiConfig::start_ap(void)
{

  Feather.printVersions();

  Serial.println("Configuring SoftAP\r\n");
  FeatherAP.err_actions(true, false);
  FeatherAP.begin(apIP, apGateway, apNetmask, WLAN_CHANNEL);
  Serial.println(board);
  Serial.println("Starting SoftAP\r\n");
  FeatherAP.start(board, WLAN_PASS, WLAN_ENCRYPTION);
  FeatherAP.printNetwork();
  //wificonfig.power_on();

  // Tell the HTTP client to auto print error codes and halt on errors
  httpserver.err_actions(true, false);

  // Configure HTTP Server Pages
  Serial.println("Adding Pages to HTTP Server");
  httpserver.addPages(pages, pagecount);

  Serial.print("Starting HTTP Server ... ");
  httpserver.begin(PORT, MAX_CLIENTS);
}






