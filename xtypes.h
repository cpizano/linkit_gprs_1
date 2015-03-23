// Linkit GPRS types.

struct RegMessage {
 int led_level;
 int iot_number;
};

struct IoTConfig {
  char did[16];
  char dom[64];
  int  cyc;
};

