#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>


// LEDs
#define led_red 4
#define led_green 2

// config lines Mics Sensor
#define enable 14
#define preheat 27
#define pin_red 35
#define pin_nox 34
int a_co;
int a_no;
int read_index = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;                // the average


// SI7006-A20 I2C address is 0x40(64)
#define Addr 0x40
float ctemp;
float humidity;
// temp calibration
#define correction 7


//MQTT settings
const char* mqtt_server = "openair.cologne.codefor.de";
char msg[250];
String data_topic = "mw/openair/";
String mqtt_id, mqtt_pass;
int mqtt_trial = 0;

/* create an instance of PubSubClient client */
WiFiClient espClient;
PubSubClient client(espClient);


//AP Setting
#define AP_SSID  "OpenAirNode"       //can set ap hostname here
String ssid_ap, pass_ap;
WiFiServer server(80);
Preferences preferences;


//settings
long lastMsg = 0;
long lastRead = 0;
long lastRestart = 0;
int heaterState = LOW;
unsigned long previousMillis = 0;        // will store last time LED was updated
long OnTime = 15000;           // milliseconds of on-time
long OffTime = 45000;          // milliseconds of off-time
int rssi;
static volatile bool wifi_connected = false;
String wifiSSID, wifiPassword;

void setup()   {
  Serial.begin(115200);
  delay(2000);
  pinMode(led_red, OUTPUT);
  pinMode(led_green, OUTPUT);
  
  //deleteApSettings();
  //deleteMqttSettings();
  WiFi.disconnect(true);
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_MODE_APSTA);
  configDeviceAP();

  //Serial.println("AP Started");
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP IPv4: ");
  Serial.println(WiFi.softAPIP());

  preferences.begin("wifi", false);
  wifiSSID =  preferences.getString("ssid", "none");           //NVS key ssid
  wifiPassword =  preferences.getString("password", "none");   //NVS key password
  preferences.end();
  Serial.print("Stored SSID: ");
  Serial.println(wifiSSID);

  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  server.begin();

  // Initialise I2C communication as MASTER
  Wire.begin(18, 19);
  // Start I2C transmission
  Wire.beginTransmission(Addr);
  // Stop I2C transmission
  Wire.endTransmission();

  //mics sensor
  pinMode(pin_nox, INPUT);
  pinMode(pin_red, INPUT);
  pinMode(preheat, OUTPUT);
  pinMode(enable, OUTPUT);
  digitalWrite(enable, HIGH);


  /* configure the MQTT server with IPaddress and port */
  client.setServer(mqtt_server, 1883);
  preferences.begin("mqtt", false);
  mqtt_id =  preferences.getString("id", "none");
  mqtt_pass =  preferences.getString("pass", "none");
  preferences.end();
  Serial.print("Stored mqtt_id: ");
  Serial.println(mqtt_id);
  Serial.print("Stored mqtt_pass: ");
  Serial.println(mqtt_pass);
  data_topic += mqtt_id;
  Serial.print("Data_Topic: ");
  Serial.println(data_topic);
}


void loop()
{
  if (wifi_connected) {
    wifiConnectedLoop();
  } else {
    wifiDisconnectedLoop();
    long currentMillis = millis();
    if (currentMillis - lastRead > 5000) {
      lastRead = currentMillis;
      digitalWrite(led_red, HIGH);
      delay(1);
      digitalWrite(led_red, LOW);
    }
    if (currentMillis - lastRestart > 15*60000) {
      ESP.restart();
    }
  }
}


//while wifi is connected
void wifiConnectedLoop() {
  long currentMillis = millis();
  if (currentMillis - lastRead > 5000) {
    lastRead = currentMillis;
    readTempHumAndCo();
    digitalWrite(led_green, HIGH);
    delay(1);
    digitalWrite(led_green, LOW);
  }
  if((heaterState == HIGH) && (currentMillis - previousMillis >= OnTime))
  {
    heaterState = LOW;
    previousMillis = currentMillis;
    turnSensorsOffAndReadOnce();
  }
  else if ((heaterState == LOW) && (currentMillis - previousMillis >= OffTime))
  {
    heaterState = HIGH;
    previousMillis = currentMillis;
    turnSensorsOn();
  }
 
  if (currentMillis - lastMsg > 60000) {
    lastMsg = currentMillis;
    sendTempHumAndCo();
    sendDustAndNo();      
  }
  if (currentMillis - lastRestart > 20*60*60000) {
    digitalWrite(preheat, LOW);
    digitalWrite(enable, LOW);
    ESP.restart(); // remove if openairnode works longer...
  }
  
}


void configDeviceAP() {
  bool result = WiFi.softAP(AP_SSID);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting");
  }
}


void WiFiEvent(WiFiEvent_t event) {
  switch (event) {

    case SYSTEM_EVENT_AP_START:
      //can set ap hostname here
      WiFi.softAPsetHostname(AP_SSID);
      //enable ap ipv6 here
      //WiFi.softAPenableIpV6();
      break;

    case SYSTEM_EVENT_STA_START:
      //set sta hostname here
      WiFi.setHostname(AP_SSID);
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      //enable sta ipv6 here
      WiFi.enableIpV6();
      break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
      //both interfaces get the same event
      Serial.print("STA IPv6: ");
      Serial.println(WiFi.localIPv6());
      Serial.print("AP IPv6: ");
      Serial.println(WiFi.softAPIPv6());
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      wifiOnConnect();
      wifi_connected = true;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      wifi_connected = false;
      wifiOnDisconnect();
      break;
    default:
      break;
  }
}
//when wifi connects
void wifiOnConnect()
{
  Serial.println("STA Connected");
  Serial.print("STA SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("STA IPv4: ");
  Serial.println(WiFi.localIP());
  Serial.print("STA IPv6: ");
  Serial.println(WiFi.localIPv6());
  WiFi.mode(WIFI_MODE_STA);     //close AP network
}

//when wifi disconnects
void wifiOnDisconnect()
{
  Serial.println("STA Disconnected");
  delay(1000);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
}

void wifiDisconnectedLoop()
{
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("New client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
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
            client.print("<html><h1>OpenAirNode Config</h1><form method='get' action='/setting'><label>SSID (Hotspot Name): "\
            "</label><br><input name='ssid' length=32><label>Passwort: </label><input name='pass' length=64><input id='mqtt_id' name='mqtt_id' style='display:none;'>" \
            "<input id='mqtt_pass' name='mqtt_pass' style='display:none;'><br><input type='submit'></form>" \
            "Hinweis: Diese Seite bleibt offen. Nachdem das Ger&auml;t die Daten empfangen hat, testet es die Verbindung.<br>Die LED blinkt gr&uuml;n, wenn alles kappt. " \
            "Wenn die Daten nach wenigen Minuten unter <a href='https://openair.cologne/sensors' target='_blank'>openair.cologne/sensors</a> nicht erscheinen, <br>" \
            "versuchen Sie Schritt 3 erneut oder wenn gar nichts merh geht - melden sie sich bei uns... <br></html>");
            client.print("<script type='text/javascript'>");
            client.print("function get_url_param( name ) { ");
            client.print("var url = document.URL; ");
            client.print("name = name.replace(/[\[]/,'\\\[').replace(/[\]]/,'\\\]'); ");
            client.print("var regexS = '[\\?&]'+name+'=([^&#]*)'; ");
            client.print("var regex = new RegExp( regexS ); ");
            client.print("var results = regex.exec(url); ");
            client.print("if( results == null ) { return ""; } else { return results[1]; } ");
            client.print("}; document.getElementById('mqtt_id').value = get_url_param('mqtt_id'); ");
            client.print("document.getElementById('mqtt_pass').value = get_url_param('mqtt_pass');</script>");
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
          continue;
        }

        if (currentLine.startsWith("GET /setting") ) {
            if (currentLine.endsWith(" HTTP/1.1")) {
            Serial.println("Cleaning old WiFi credentials from ESP32");
            // Remove all preferences under opened namespace
            preferences.clear();
            String s = currentLine;
            int ssid_start_index = s.indexOf("ssid=");
            int ssid_end_index = s.indexOf("&", ssid_start_index + 1);
            int pass_start_index = s.indexOf("pass=", ssid_end_index + 1);
            int pass_end_index = s.indexOf("&",  pass_start_index + 1);
            int id_start_index = s.indexOf("mqtt_id=", pass_end_index + 1);
            int id_end_index = s.indexOf("&", id_start_index + 1);
            int mqtt_pass_start_index = s.indexOf("mqtt_pass=", id_end_index + 1);
            int mqtt_pass_end_index = s.indexOf(" HTTP", mqtt_pass_start_index + 1);
            ssid_ap = urlDecode(s.substring(ssid_start_index + 5, ssid_end_index));
            pass_ap = urlDecode(s.substring(pass_start_index + 5, pass_end_index));
            mqtt_id = urlDecode(s.substring(id_start_index + 8, id_end_index));
            mqtt_pass = urlDecode(s.substring(mqtt_pass_start_index + 10, mqtt_pass_end_index));
            Serial.println("params:");
            Serial.println(ssid_ap);
            Serial.println(pass_ap);
            Serial.println(mqtt_id);
            Serial.println(mqtt_pass);

            preferences.begin("wifi", false); // Note: Namespace name is limited to 15 chars
            Serial.println("Writing new ssid");
            preferences.putString("ssid", ssid_ap);
  
            Serial.println("Writing new pass");
            preferences.putString("password", pass_ap);
            delay(300);
            preferences.end();
            if ((mqtt_id.length() > 0) && (mqtt_pass.length() > 0)) {
              preferences.begin("mqtt", false); // Note: Namespace name is limited to 15 chars
              Serial.println("Writing new id");
              preferences.putString("id", mqtt_id);
    
              Serial.println("Writing new pass");
              preferences.putString("pass", mqtt_pass);
              delay(300);
              preferences.end();
            }
            delay(5000);
            ESP.restart();
          }
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

void mqttconnect() {
  /* Loop until reconnected */
  while (!client.connected()) {
    Serial.print("MQTT connecting ...");
    /* connect now */
    if (client.connect(mqtt_id.c_str(),mqtt_id.c_str(),mqtt_pass.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, status code =");
      Serial.print(client.state());
      Serial.println("try again in seconds");
      if (mqtt_trial > 5) {
        ESP.restart();
      }
      mqtt_trial += 1;
      /* Wait 1 second before retrying */
      delay(1000);
    }
  }
}

void readTempHumAndCo() {
    unsigned int data[2];
  
    // Start I2C transmission
    Wire.beginTransmission(Addr);
    // Send humidity measurement command, NO HOLD MASTER
    Wire.write(0xF5);
    // Stop I2C transmission
    Wire.endTransmission();
    delay(500);
  
    // Request 2 bytes of data
    Wire.requestFrom(Addr, 2);
  
    // Read 2 bytes of data
    // humidity msb, humidity lsb
    if (Wire.available() == 2)
    {
      data[0] = Wire.read();
      data[1] = Wire.read();
    }
  
    // Convert the data
    humidity  = ((data[0] * 256.0) + data[1]);
    humidity = ((125 * humidity) / 65536.0) - 6;
  
    // Start I2C transmission
    Wire.beginTransmission(Addr);
    // Send temperature measurement command, NO HOLD MASTER
    Wire.write(0xF3);
    // Stop I2C transmission
    Wire.endTransmission();
    delay(500);
  
    // Request 2 bytes of data
    Wire.requestFrom(Addr, 2);
  
    // Read 2 bytes of data
    // temp msb, temp lsb
    if (Wire.available() == 2)
    {
      data[0] = Wire.read();
      data[1] = Wire.read();
    }
  
    // Convert the data
    float temp  = ((data[0] * 256.0) + data[1]);
    ctemp = (((175.72 * temp) / 65536.0) - 46.85) - correction;
  
    // Output data to serial monitor
    Serial.print("Relative humidity : ");
    Serial.print(humidity);
    Serial.println(" % RH");
    Serial.print("Temperature in Celsius : ");
    Serial.print(ctemp);
    Serial.println(" C");

    total += analogRead(pin_red);
    read_index += 1;
}


void sendTempHumAndCo() {
  if (wifi_connected) {  
    if (!client.connected()) {
      mqttconnect();
    }
    int rssi = WiFi.RSSI();
    a_co = total/read_index;
    total = 0;
    read_index = 0;
    snprintf (msg, 250,"{\"temp\": \"%.2f\",\"hum\": \"%.2f\",\"r2\": \"%i\"}", ctemp, humidity, a_co);
    Serial.println(msg);
    client.publish(data_topic.c_str(), msg);
    a_co = 0;
  }
}

void sendDustAndNo() {
  if (wifi_connected) {
   if (!client.connected()) {
      mqttconnect();
    }
    rssi = WiFi.RSSI();
    snprintf (msg, 250,"{\"rssi\": \"%i\",\"r1\": \"%i\" }", rssi, a_no);
    Serial.println(msg);
    Serial.println(data_topic);
    client.publish(data_topic.c_str(), msg); 
    a_no = 0;
  }
}

void turnSensorsOffAndReadOnce() {
  Serial.println("Preheat off and read");
  a_no = analogRead(pin_nox);
  digitalWrite(preheat, LOW);
}

void turnSensorsOn() {
  Serial2.begin(9600);
  Serial.println("preheat on");
  digitalWrite(preheat, HIGH);
}

String urlDecode(String text) {
  String decoded = "";
  char temp[] = "0x00";
  unsigned int len = text.length();
  unsigned int i = 0;
  while (i < len)
  {
    char decodedChar;
    char encodedChar = text.charAt(i++);
    if ((encodedChar == '%') && (i + 1 < len))
    {
      temp[2] = text.charAt(i++);
      temp[3] = text.charAt(i++);

      decodedChar = strtol(temp, NULL, 16);
    }
    else {
      if (encodedChar == '+')
      {
        decodedChar = ' ';
      }
      else {
        decodedChar = encodedChar;  // normal ascii char
      }
    }
    decoded += decodedChar;
  }
  return decoded;
}

void deleteApSettings() {
  preferences.begin("wifi", false); // Note: Namespace name is limited to 15 chars
  preferences.putString("ssid", "");
  preferences.putString("password", "");
  delay(300);
  preferences.end();
}

void deleteMqttSettings() {
  preferences.begin("mqtt", false); // Note: Namespace name is limited to 15 chars
  preferences.putString("id", "");
  preferences.putString("pass", "");
  delay(300);
  preferences.end();
}
