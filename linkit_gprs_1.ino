// Linkit ONE proto code. Copyright 2015 Carlos Pizano.
// This code assumes you have SeedStudio ledbar and a functioning
// GPRS sim card.

#include <LBattery.h>

#include <Suli.h>
#include <Wire.h>
#include <Seeed_LED_Bar_Arduino.h>
#include <LTask.h>
#include <LGPRS.h>
#include <LGPRSClient.h>
#include <LFlash.h>

// Apparently arduino insists new types to be defined in a separate file.
#include "xtypes.h"

#define SVC_URL "erudite-syntax-729.appspot.com"
//#define SER_DBG 1

SeeedLedBar bar(9, 8);
LBatteryClass battery;
VMCHAR firmware_datetime[64] = {0};
IoTConfig iot_config;
int cycle_counter = 0;

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
  // Executed in another thread. Don't do Arduino stuff there.
  vm_get_sys_property(MRE_SYS_BUILD_DATE_TIME,  firmware_datetime, sizeof(firmware_datetime));
  return true;
}

int LightSensorRead() {
  int x0 = analogRead(A0);
  delay(5);
  int x1 = analogRead(A0);
  return (x0 + x1) / 2;
}

bool DoRegRequest(LGPRSClient& client, const char* status_s, int battery, int cycles) {
  client.printf("GET /reg?did=%s&sta=%s&bat=%d&cyc=%d HTTP/1.1\n",
                iot_config.did, status_s, battery, cycles);
  client.printf("Host: " SVC_URL ":80\n");
  client.printf("User-Agent: LinkIt(%s)IoT\n", firmware_datetime);
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

int CopyCfgValue(const char* key_name, const char* in_buf, char* out_buf, int sz) {
  char* pos_a = strstr(in_buf, key_name);
  if (!pos_a) {
    DebugOut("=no keyname");
    return 0;
  }
  char* pos_b = strchr(pos_a, '\n');
  if (pos_b < pos_a) {
    DebugOut("=cfg corrupt");
    return 0;     
  }
  pos_a += strlen(key_name);
  int delta = pos_b - pos_a;
  if (delta >= sz) {
    DebugOut("=bad cfg value sz");
    return 0;
  }
  memcpy(out_buf, pos_a, delta);
  memset(out_buf + delta, 0, sz - delta);
  return delta;
}

bool ProcessConfig(const char* filename, IoTConfig* iot_config) {
  LFile f = LFlash.open(filename, FILE_READ);
  if (!f) {
    DebugOut("=missing flash file");
    return false;    
  }

  f.seek(0);
  const int sz = 1024 * 2;
  char* buf = new char[sz];
  f.read(buf, sz);
  
  const char header[] = "IoTConfig0\n";
  char* root_pos = strstr(buf, header);
  if (!root_pos) {
    DebugOut("=bad config header");
    return false; 
  }

  root_pos += sizeof(header) - 1;
  CopyCfgValue("@did:", root_pos, iot_config->did, sizeof(iot_config->did));
  DebugOut(iot_config->did);
  CopyCfgValue("@dom:", root_pos, iot_config->dom, sizeof(iot_config->dom));
  DebugOut(iot_config->dom);

  delete buf;
  return true;
}

void setup() {
  Serial.begin(9600);
  bar.begin(9, 8);
  bar.setLevel(1);

  LTask.begin();
  LFlash.begin();
  
  // Get the firmware version.
  LTask.remoteCall(&GetFirmwareBuild, NULL);

  // Wait for the terminal app to send any key.
  DebugWait();  
  DebugOut("v 0.0.4c ready ");

  // Ready key parameters from the config file.
  // the config file is at the root of the drive and can be
  // written when mounting the LinkIt as a flash drive.
  ProcessConfig("iot.cfg", &iot_config);
  
  while(!LGPRS.attachGPRS("epc.tmobile.com", NULL, NULL)) {
    DebugOut("wait for SIM card");
    delay(500);
  }
  
  delay(3000);
  bar.setLevel(2);
  DebugOut("init done.");
}

void loop() {
  ++cycle_counter;
  DebugOut("loop");

  delay(20000);
  LGPRSClient client;
  
  if(!client.connect(SVC_URL, 80)) {
    bar.indexBit(0b000001000000001);
    DebugOut("=error connecting server");
    return;
  }
  
  LightSensorRead();
 
  DoRegRequest(client, "ready", battery.level(), cycle_counter);
  delay(2500);
  
  char* message = ReadMessage(client);
  DebugOut(message);  
  if (!message) {
    bar.indexBit(0b000001010000001); 
    DebugOut("=error reading server");
    return; 
  }

  RegMessage rm;
  if (!ProcessMessage(message, &rm)) {
    DebugOut("=wrong message");
    delete message;
    return; 
  }
 
  delete message;
  bar.setLevel(rm.led_level);
  delay(2000);
  bar.setLevel(0);
}

