#include "DHT.h"

#include <LBattery.h>

#include <Suli.h>
#include <Wire.h>
#include <Seeed_LED_Bar_Arduino.h>
#include <LTask.h>
#include <LGPRS.h>
#include <LGPRSClient.h>
#include "xtypes.h"

#define DHTPIN 3
#define DHTTYPE DHT22

#define SVC_URL "erudite-syntax-729.appspot.com"
#define DEVICE_ID "did=23401"
#define SER_DBG 1

SeeedLedBar bar(9, 8);
DHT dht(DHTPIN, DHTTYPE);
LBatteryClass battery;
VMCHAR build_datetime[64] = {0};

void DebugOut(const char* txt) {
#if defined(SER_DBG)
  Serial.println(txt);
#endif
}

void DebugWait() {
#if defined(SER_DBG)
  while (Serial.available() == 0) {
    delay(20);  
  }
  char ss = Serial.read();
#endif
}

boolean GetFirmwareBuild(void*) {
  vm_get_sys_property(MRE_SYS_BUILD_DATE_TIME,  build_datetime, sizeof(build_datetime));
  return true;
}

int LightSensorRead() {
  int x0 = analogRead(A0);
  delay(5);
  int x1 = analogRead(A0);
  return (x0 + x1) / 2;
}

bool DoRegRequest(LGPRSClient& client, const char* status_s, int battery, int cycles) {
  client.printf("GET /reg?" DEVICE_ID "&sta=%s&bat=%d&cyc=%d HTTP/1.1\n",
                status_s, battery, cycles);
  client.printf("Host: " SVC_URL ":80\n");
  client.printf("User-Agent: LinkIt(%s)IoT\n", build_datetime);
  client.println();
  return true;
}

char* ReadMessage(LGPRSClient& client) {
  if (!client.available())
    return NULL;    
  char* buf = new char[1024];
  int v = client.read(reinterpret_cast<uint8_t*>(buf), 1024);
  if ((v < 0) || (v == 1024)) {
    delete buf;
    return false;
  }
  buf[v] = 0;
  return buf;
}

bool ProcessMessage(const char* message, RegMessage* rm) {
  char* iot = strstr(message, "IoT 0");
  if (!iot)
    return false;
  rm->iot_number = atoi(&iot[4]);
  char* led = strstr(iot, "ledbar");
  if (!led)
    return false;
  rm->led_level = atoi(&led[7]);
  return true;  
}

void setup() {
  Serial.begin(9600);
  bar.begin(9, 8);
  dht.begin();
  
  LTask.remoteCall(&GetFirmwareBuild, NULL);
  bar.setLevel(1);

  DebugWait();  
  Serial.print("v 0.0.3 ready ");
  Serial.println(build_datetime);
  
  while(!LGPRS.attachGPRS("epc.tmobile.com", NULL, NULL)) {
    DebugOut("wait for SIM card");
    delay(500);
  }
  
  delay(3000);
  DebugOut("init done.");
  bar.setLevel(2);
}

int cycle_counter = 0;

void loop() {
  ++cycle_counter;
  DebugOut("loop");

  delay(20000);
  LGPRSClient client;
  
  if(!client.connect(SVC_URL, 80)) {
    bar.indexBit(0b000001000000001);
    DebugOut("== error connecting server");
    return;
  }
  
  DebugOut(LightSensorRead());
  
  float temp = 0.0;
  float humt = 0.0;
  if (dht.readHT(&temp, &humt)) {
    Serial.print("temp = ");
    Serial.println(temp);
    Serial.print("humidity = ");
    Serial.println(humt);
  } else {
    Serial.println("not t&h data"); 
  }

  DoRegRequest(client, "ready", battery.level(), cycle_counter);
  delay(2500);
  
  char* message = ReadMessage(client);
  DebugOut(message);
  
  if (!message) {
    bar.indexBit(0b000001010000001); 
    DebugOut("== error reading server");
    return; 
  }

  RegMessage rm;
  if (!ProcessMessage(message, &rm)) {
    DebugOut("== wrong message");
    delete message;
    return; 
  }
 
  delete message;
  bar.setLevel(rm.led_level);
  delay(2000);
  bar.setLevel(0);
}

