#include "WifiConfig.h"
#include "DataApi.h"
#include "OpenAirSensor.h"
#include "config.h"

// config lines 
#define en PA0 //or PA0
#define pre PC3
#define vred PC2
#define vnox PA3

const int port = 80;
const char* host = "influx.datacolonia.de";

float NO2_ppm;
float CO_ppm;
int r_ox;
int r_co;
//String data = "";
char data[256];

WifiConfig wificonfig;
OpenAirSensor sensor(en, pre, vred, vnox);
DataApi data_api(host, port);

void setup()
{
  Serial.begin(115200);

  //sensor.change_resolution(4096); //change to your current resolution steps of the ADC;
  pinMode(pre, OUTPUT);
  pinMode(en, OUTPUT);
  pinMode(vnox, INPUT_ANALOG);
  pinMode(vred, INPUT_ANALOG);
  // Wait for the serial port to connect. Needed for native USB port only.
  //while (!Serial) delay(1);

  delay(6000);
  sensor.power_on();
  wificonfig.initialize();
}


void loop()
{
  delay(10000);
  data[0] = (char)0;
  sensor.heater_on();
  delay(30000);
  NO2_ppm = sensor.NO2_ppm();
  CO_ppm = sensor.CO_ppm();
  Serial.println(NO2_ppm);
  sensor.heater_off();
  
  Serial.println();
  Serial.println(sensor.a_ox);
  Serial.println(sensor.r_ox);
  Serial.println(sensor.a_co);
  Serial.println(sensor.r_co);
  // influxDB data format: key,tag_key=param field=param

  snprintf(data, sizeof data, 
    "r_no,id=%s,mac=%s value=%.2f \n"
    "sens_no,id=%s,mac=%s value=%d \n"
    "r_co,id=%s,mac=%s value=%.2f \n"
    "sens_co,id=%s,mac=%s value=%d \n",
    wificonfig.board, wificonfig.api_key, sensor.r_ox,
    wificonfig.board, wificonfig.api_key, sensor.a_ox,
    wificonfig.board, wificonfig.api_key, sensor.r_co,
    wificonfig.board, wificonfig.api_key, sensor.a_co);
  if (wificonfig.connectable) {
    while ( !data_api.connect_ap() )
    {
      delay(2000); // delay between each attempt
    }
    Serial.print(data);
    data_api.send_to_influx(data, url);
    delay(60000*15); 
  }
}
