#include <WiFi101.h>


// Feather M0 / WiFi (ATWINC)
//
// PINOUT
//              RST
//              3V
//              Aref
// BAT          GND
// EN           A0 (DAC)
// USB          A1
// 13 (LED)     A2
// 12           A3
// 11           A4
// 10           A5
//  9 (A7)      24 (SCK)
//  6           23 (MOSI)
//  5           22 (MISO)
// 21 (SCL)      0 (RX)
// 20 (SDA)      1 (TX)
//              Wake

// MiCS 4514
// V33 -
// nc  -
// GND -
// PRE  - A0
// EN   - A1
// Vred - A2
// Vnox - A3

const char ssid[] = "marienUniversum";
const char pswd[] = "asdfghjkllkjhgfdsa";

static const int BAUD = 9600;

// XX:XX:XX:XX:XX:XX\000
static char mac[18];
static WiFiClient client;

void blink(uint n = 1);
void log(char * msg);
bool waitForWiFi(uint numAttempts = 5, uint wait = 10000);
void populateMAC();
void readSensors();
void write_sensors();

void setup() {
  // put your setup code here, to run once:
  WiFi.setPins(8, 7, 4, 2);
  Serial.begin(BAUD);
  // configure pins
  configurePins();
  blink();
  delay(1000); // Arduino folklore suggests adding while (!Serial) to
  // to wait for init, but that block indefinitely if no
  // USB Serial is connected, or something.

  Serial.println("Welcome!");
  blink(2);
  // wait for WiFi
  if (!waitForWiFi()) {
    while (true) blink(10);
  }
  blink(3);
  populateMAC();
  // establish server tcp connection

  if (!client.connect("192.168.178.37", 5012)) {
    log("can't connect");
    while (true) blink(10);
  }

}

long lastRead = 0;
const uint WAIT_EN = 5000; // number of milliseconds to wait before
const uint READ_INTERVAL = 500;

static const uint PRE    = A0;
static const uint EN     = A1;
static const uint V_RED  = A2;
static const uint V_NOX  = A3;
static const uint LED    = 13;

void loop() {
  // put your main code here, to run repeatedly:

  long now = millis();
  if (now - lastRead >= READ_INTERVAL) {
    readSensors();

    if (now >= WAIT_EN) {
      // wait for a while before turning on heater.
      digitalWrite(EN, HIGH);
      digitalWrite(LED, HIGH);
    }

    lastRead = now;
  }
}




// stm32/wicket needs INPUT_ANALOG
static const uint ANALOG_INPUT = INPUT;

void configurePins() {
  // initialize sensors ...
  pinMode(EN, OUTPUT);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  pinMode(V_RED, INPUT);
  pinMode(V_NOX, INPUT);
}

void write_sensors(int vred, int vnox) {

  // 192.168.179.44
  // 5012
  char str [72];
  sprintf(str, "%d %s %d %d", millis(), mac, vred, vnox);
  log(str);
  client.println(str);
} 
void readSensors() {
  int vnox = analogRead(V_NOX);
  int vred = analogRead(V_RED);
  write_sensors(vred, vnox);
}

void blink (uint n) {
  for (; n != 0;) {
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    --n;
    if (n != 0) {
      delay(100);
    }
  }
}

void log(char * msg) {
  static char str[72];
  sprintf(str, "%d : %s", millis(), msg);
  Serial.println(str);
}
bool waitForWiFi(uint numAttempts, uint wait) {
  if (numAttempts == 0) {
    return false;
  }
  WiFi.begin(ssid, pswd);
  int status = WiFi.status();
  Serial.println(status);
  switch (status) {
    case WL_CONNECTED:           //  3
      return true;
    case WL_IDLE_STATUS:         //  0
      log("WL_IDLE_STATUS");
      delay(wait);
      return waitForWiFi(numAttempts - 1, wait);
    case WL_CONNECT_FAILED:      //  4
      log("WL_CONNECT_FAILED");
      delay(wait);
      return waitForWiFi(numAttempts - 1, wait);
    case WL_NO_SHIELD:           //255
    case WL_NO_SSID_AVAIL:       //  1
    case WL_SCAN_COMPLETED:      //  2
    case WL_CONNECTION_LOST:     //  5
    case WL_DISCONNECTED:        //  6
      Serial.println("failed permantly, check params");
      return false;
    default :
      log("failed mysteriously");
      return false;
  }
}

void populateMAC() {
  byte m[6];
  WiFi.macAddress(m);
  mac[17] = 0;
  sprintf(mac, "%x:%x:%x:%x:%x:%x", m[5], m[4], m[3], m[2], m[1], m[0]);
  log(mac);
}


