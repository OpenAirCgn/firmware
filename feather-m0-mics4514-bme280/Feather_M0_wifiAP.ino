/*
  Library for NO2 and CO Sensor reading.
  Created by Marcel Belledin, October 27, 2016.
  Modified by Reinhard Nickels with bme280 sensor
*/

#include <SPI.h>
#include <WiFi101.h>
#include <FlashStorage.h>
#include "config.h" // <- in this file you should define user and password of the influx db. Ask Marcel to get one.

/***************************************************************************
  This is a library for the BME280 humidity, temperature & pressure sensor

  Designed specifically to work with the Adafruit BME280 Breakout
  ----> http://www.adafruit.com/products/2650

  These sensors use I2C or SPI to communicate, 2 or 4 pins are required
  to interface.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit andopen-source hardware by purchasing products
  from Adafruit!

  Written by Limor Fried & Kevin Townsend for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ***************************************************************************/
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1024)   // generic
Adafruit_BME280 bme; // I2C

// feather m0 config
#define en A0
#define pre A1
#define vred A2
#define vnox A3
#define in_type INPUT
int resolution = 1023; // resolution steps of the ADC

// BME 280 config - Sensor breakout is connected to SDA,SCL,5,6 digital I/O pins are switched to GND and VCC
#define bme_gnd 5
#define bme_vcc 6
#define bme_i2c_addr 0x76
boolean bme_present = false;

// ap storage config
typedef struct {
  boolean valid;
  char ssid_ap[100];
  char pass_ap[100];
} ap;

FlashStorage(my_ap, ap);

// Create a "Access Point" variable and call it "owner"
ap owner;

// local ap config
char ssid[] = "isGenerated"; // created AP name
char pass[] = "1234567890"; // AP password (needed only for WEP, must be exactly 10 or 26 characters in length)
String ssid_ap;
String pass_ap;
int keyIndex = 0; // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;
WiFiServer server(80);

// data push config
//const int port = 9086;
//const char* host = "138.201.88.0";
const int port = 80;
const char* host = "influx.datacolonia.de";
String data = "";
char board[6];
char api_key[12];
bool ap_connectable = false;
bool ap_created = false;

// influxdb
bool pingable = false;
WiFiClient influx_client;

// sensor config
float board_volt = 3.3; // input voltage
float vout_ox = 0; // output voltage
float vout_co = 0; // output voltage
float r5 = 3900; // RLOAD_OX in Ohm
float r7 = 360000; // RLOAD_CO in Ohm


// vars for ppm calculation
float ratio_ox = 0;
float ppm_ox = 0;
float ratio_co = 0;
float ppm_co = 0;
float r0_ox = 900; // assumed resistance in fresh air... to be calibrated
float r0_co = 200; // assumed resistance in fresh air... to be calibrated
float r_ox = 0; // calculated resistance
float r_co = 0; // calculated resistance
int a_ox;
int a_co;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  //while (!Serial) {
  //  ; // wait for serial port to connect. Needed for native USB port only
  //}
  WiFi.setPins(8, 7, 4, 2);
  WiFi.maxLowPowerMode(); // go into power save mode
  //sensor:
  pinMode(pre, OUTPUT);
  pinMode(en, OUTPUT);
  // bme280 sensor
  pinMode(bme_gnd, OUTPUT);
  pinMode(bme_vcc, OUTPUT);
  digitalWrite(bme_gnd, LOW);
  digitalWrite(bme_vcc, HIGH);
  // Read the content of "my_ap" into the "owner" variable
  owner = my_ap.read();
  Serial.println(owner.ssid_ap);

  if (owner.valid) {
    connect_ap();
  } else {
    search_connect_freifunk();
    
    // the above did not work
    if (ap_connectable == false){
      create_ap;
    }
  }
  if (!bme.begin(bme_i2c_addr)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    bme_present = false;
  }
  else {
    Serial.println("Found BME280 sensor, OK");
    delay(1000);   // wait for vcc stability
    bme_present = true;
    // take 2 measurements to empty buffer
    for (int i = 0; i < 2; i++) {
      Serial.print("Temperature = ");
      Serial.print(bme.readTemperature());
      Serial.println(" *C");
      Serial.print("Pressure = ");
      Serial.print(bme.readPressure() / 100.0F);
      Serial.println(" hPa");
      Serial.print("Approx. Altitude = ");
      Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
      Serial.println(" m");
      Serial.print("Humidity = ");
      Serial.print(bme.readHumidity());
      Serial.println(" %");
      Serial.println();
    }
  }
}  // end of setup

void create_ap() {
  Serial.println("Access Point Web Server");
  WiFi.end();

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue
    while (true);
  }

  // print the network name (SSID);
  Serial.print("Creating access point named: ");
  byte mac[6];
  WiFi.macAddress(mac);
  sprintf(ssid, "%02X%02X%02X%02X%02X%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  Serial.println(ssid);
  
  // Create open network. Change this line if you want to create an WEP network:
  status = WiFi.beginAP(ssid);
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // don't continue
    while (true);
  }

  // wait 5 seconds for connection:
  delay(5000);

  // start the web server on port 80
  server.begin();

  // you're connected now, so print out the status
  printWifiStatus();
  ap_created = true;
}

void get_new_ssid() {
  // compare the previous status to the current status
  if (status != WiFi.status()) {
    // it has changed update the variable
    status = WiFi.status();

    if (status == WL_AP_CONNECTED) {
      // a device has connected to the AP
      Serial.println("Device connected to AP");
    } else {
      // a device has disconnected from the AP, and we are back in listening mode
      Serial.println("Device disconnected from AP");
    }
  }

  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("new client");           // print a message out the serial port
    bool new_owner = false;
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        //Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print("<html><h1>OpenAir Sensor Config</h1><form method='get' action='setting'><label>SSID (Name des Hotspots): </label><input name='ssid' length=32><label>Passwort: </label><input name='pass' length=64><input type='submit'></form>");
            client.print("Hinweis: Wir ben&ouml;tigen den WLAN Zugang, um die Sensormessungen an unsere Datenbank zu senden. Nachdem das Ger&auml;t die Daten empfangen hat, testet es die Verbindung. ");
            client.print("Ihr Browser zeigt dabei eine Fehlerseite...Wenn die Daten nach wenigen Minuten unter <a href=\"http://openair.codingcologne.de\">openair.codingcologne.de</a> nicht erscheinen, melden sie sich. (siehe Anleitung)");
            client.print("</html>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          }
          else {      // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }
        else if (c != '\r') {    // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
        // Check to see if the client request was "GET /H" or "GET /L":
        if (currentLine.startsWith("GET /setting")) {
          if (currentLine.endsWith(" HTTP")) {
            String s = currentLine;
            int firstIndex = s.indexOf("ssid=");
            int secondIndex = s.indexOf('&', firstIndex + 1);
            int thirdIndex = s.indexOf("pass=", secondIndex + 1);
            int endIndex = s.indexOf(" HTTP", thirdIndex + 1);
            ssid_ap = escapeParameter(s.substring(firstIndex + 5, secondIndex));
            pass_ap = escapeParameter(s.substring(thirdIndex + 5, endIndex)); // To the end of the string
            Serial.println("params:");
            Serial.println(ssid_ap);

            // Fill the "owner" structure with the data entered by the user...
            owner.valid = true;
            ssid_ap.toCharArray(owner.ssid_ap, 100);
            pass_ap.toCharArray(owner.pass_ap, 100);

            // ...and finally save everything into "my_ap"
            my_ap.write(owner);
            ap_created = false;
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print("<html><h1>Wir versuchen zu verbinden...</h1>");
            client.print("Die Verbindung wird gleich getrennt... <a href=\"http://192.168.1.1\"> Falls keine Daten auf openair.datacolonia.de erscheinen die Anleitung und diesem Link folgen...</a>");
            client.print("</html>");

            // The HTTP response ends with another blank line:
            client.println();
            client.stop();
            delay(10000);
            connect_ap();
          }
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}


void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.println(ip);


}

void search_connect_freifunk() {
  String myssid;
  int attempts = 0;
  // scan for nearby networks:
  Serial.println("** Scan Networks **");
  byte numSsid = WiFi.scanNetworks();

  // print the list of networks seen:
  Serial.print("number of available networks:");
  Serial.println(numSsid);

  // print the network number and name for each network found:
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    myssid = WiFi.SSID(thisNet);
    if ( myssid == "kbu.freifunk.net" ) {
      Serial.print("Freifunk SSID: ");
      Serial.print(FREIFUNKSSID);
      Serial.println(" found!");
       while ( (status != WL_CONNECTED) && (attempts < 5) ) {
        Serial.println("Attempting to connect...");
        status = WiFi.begin(myssid);
        attempts += 1;
        // wait 5 seconds for connection:
        delay(5000);
      }
      printWifiStatus();
      //
      if (attempts > 4) {
        ap_connectable = false;
        WiFi.end();
        create_ap();
      } else {
        ap_connectable = true;
        check_pingable();
      }
    }
  }
}

void connect_ap() {
  WiFi.end();
  int attempts = 0;
  while ( (status != WL_CONNECTED) && (attempts < 5) ) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(owner.ssid_ap);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(owner.ssid_ap, owner.pass_ap);
    attempts += 1;
    // wait 5 seconds for connection:
    delay(5000);
  }
  printWifiStatus();
  //
  if (attempts > 4) {
    ap_connectable = false;
    WiFi.end();
    create_ap();
  } else {
    ap_connectable = true;
    check_pingable();
  }
}

char* get_board() {
  byte mac[6];
  WiFi.macAddress(mac);
  sprintf(board, "%02X%02X%02X", mac[2], mac[1], mac[0]);
  return board;
}

char* get_api_key() {
  byte mac[6];
  WiFi.macAddress(mac);
  sprintf(api_key, "%02X%02X%02X%02X%02X%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  return api_key;
}

void send_to_influx() {
  float bme_temp, bme_hum, bme_press;
  if (!bme.begin(bme_i2c_addr)) {   // seems the sensor need re-initialization ?
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }
  else {
    Serial.println("Found BME280 sensor, OK");
  }
  data = "";
  if (bme_present) {
    Serial.print("preHeat\t");
    bme_temp = get_bme_temp();
    Serial.print(bme_temp);
    Serial.print(" °C\t");
    bme_hum = get_bme_hum();
    Serial.print(bme_hum);
    Serial.print(" %\t");
    bme_press = get_bme_press();
    Serial.print(bme_press);
    Serial.println(" mBar");
  }
  digitalWrite(en, HIGH);
  delay(10000);
  // read the value from the sensor:
  digitalWrite(pre, 1);
  delay(30000);
  if (bme_present) {
    Serial.print("postHeat\t");
    Serial.print(get_bme_temp());
    Serial.print(" °C\t");
    Serial.print(get_bme_hum());
    Serial.print(" %\t");
    Serial.print(get_bme_press());
    Serial.println(" mBar");
  }
  Serial.println(get_ox_resistane());
  Serial.println(get_co_resistane());
  // influxDB data format: key,tag_key=param field=param
  data += "r_no,id=" + String(get_board()) + ",mac=" + String(get_api_key()) + " value=" + String(r_ox) + " \n";
  data += "sens_no,id=" + String(get_board()) + ",mac=" + String(get_api_key()) + " value=" + String(a_ox) + " \n";
  data += "r_co,id=" + String(get_board()) + ",mac=" + String(get_api_key()) + " value=" + String(r_co) + " \n";
  data += "sens_co,id=" + String(get_board()) + ",mac=" + String(get_api_key()) + " value=" + String(a_co) + " \n";
  if (bme_present) {
    data += "bme_t,id=" + String(get_board()) + ",mac=" + String(get_api_key()) + " value=" + String(bme_temp) + " \n";
    data += "bme_h,id=" + String(get_board()) + ",mac=" + String(get_api_key()) + " value=" + String(bme_hum) + " \n";
    data += "bme_p,id=" + String(get_board()) + ",mac=" + String(get_api_key()) + " value=" + String(bme_press) + " \n";
  }

  Serial.println(data);
  if (!influx_client.connect(host, port)) {
    Serial.println("connection failed");
    return;
  }
  // send request to the server
  influx_client.print(String("POST ") + "/write?db=open_air&u=" + String(USER) + "&p=" + String(PASS) + " HTTP/1.1\r\n" +
                      "Host: " + host + "\r\n" +
                      "Content-Type: application/x-www-form-urlencoded\r\n");
  influx_client.print("Content-Length: ");
  influx_client.println(data.length(), DEC);
  influx_client.println("Connection: close\r\n");

  influx_client.print(data);

  delay(1);

  unsigned long timeout = millis();
  while (influx_client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      influx_client.stop();
      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  while (influx_client.available()) {
    String line = influx_client.readStringUntil('\r');
    Serial.print(line);
  }

  Serial.println();
  Serial.println("closing connection");
  digitalWrite(pre, 0);
  delay(60000 * 15);
}

void check_pingable() {
  int pingResult = WiFi.ping(host);

  if (pingResult >= 0) {
    Serial.println("SUCCESS!");
    pingable = true;
  } else {
    Serial.print("FAILED! Error code: ");
    Serial.println(pingResult);
    pingable = false;
  }

}

float get_ox_resistane() {
  a_ox = analogRead(vnox);
  vout_ox = (board_volt / resolution) * a_ox; // Calculates the Voltage
  r_ox = ((board_volt - vout_ox) * r5) / vout_ox; // Calculates the resistance
  return isnan(r_ox) ? -1 : r_ox;
}

float get_co_resistane() {
  a_co = analogRead(vred);
  vout_co = (board_volt / resolution) * a_co; // Calculates the Voltage
  r_co = ((board_volt - vout_co) * r7) / vout_co; // Calculates the resistance
  return isnan(r_co) ? -1 : r_co;
}

float get_bme_temp() {
  return bme.readTemperature();
}

float get_bme_hum() {
  return bme.readHumidity();
}

float get_bme_press() {
  return (bme.readPressure() / 100.0F);
}

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
void loop() {
  if (pingable) {
    send_to_influx();
  } else {
    if ((ap_connectable == false) && (ap_created == true)) {
      get_new_ssid();
    }
  }

}

