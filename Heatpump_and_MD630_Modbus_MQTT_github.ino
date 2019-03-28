#include <SimpleModbusMaster.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Network and MQTT server settings
const char* ssid = "yourSSID";
const char* password = "yourPassword";
const char* MQTT_SERVER = "192.168.x.x";
const char* MQTT_CLIENTID = "ESP_HeatPump_modbus";
const char* MQTT_USERNAME = "username";
const char* MQTT_PASSWORD = "password";

// MQTT topics
String outTopic = "/boilerroom/1_esp1/out/";
String inTopic = "/boilerroom/1_esp1/in/";

WiFiClient espClient;
PubSubClient client(espClient);

long longInterval = 10000;   // 10 second update interval
long lastEmonCMSMsg = 0;

long shortInterval = 500; //0.5 second update interval
long lastMsg = 0;

// ModBUS settings
#define baud 9600
#define timeout 500
#define polling 250 // the scan rate
#define retry_count 9999999999

// used to toggle the receive/transmit pin on the ModBUS driver
#define TxEnablePin D2

// The total amount of available memory on the master to store data
#define TOTAL_NO_OF_REGISTERS 65

// This is the easiest way to create new packets
// Add as many as you want. TOTAL_NO_OF_PACKETS
// is automatically updated.
enum
{
  PACKET1,
  PACKET2,
  PACKET3,
  PACKET4,
  PACKET5,
  PACKET6,
  PACKET7,
  PACKET8,
  PACKET9,
  PACKET10,
  PACKET11,
  PACKET12,
  PACKET13,
  PACKET14,
  PACKET15,
  PACKET16,
  TOTAL_NO_OF_PACKETS // leave this last entry
};

// Create an array of Packets to be configured
Packet packets[TOTAL_NO_OF_PACKETS];

// Masters register array
unsigned int regs[TOTAL_NO_OF_REGISTERS];
unsigned int regs_p[TOTAL_NO_OF_REGISTERS];


void setup()
{

  setup_wifi();

  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);


  // Initialize each packet for HEATPUMP RDT-W modbus interface
  //READ READ-ONLY PARAMETERS
  modbus_construct(&packets[PACKET1], 2, READ_INPUT_REGISTERS, 50, 1, 1);    // 1 - Remcon Room temperature
  modbus_construct(&packets[PACKET2], 2, READ_INPUT_REGISTERS, 70, 3, 2);    // 2 - Space heating ON/OFF, 3 - Circulation pump ON/OFF, 4 - Compressor ON/OFF
  modbus_construct(&packets[PACKET3], 2, READ_INPUT_REGISTERS, 74, 5, 5);    // 5 - Disinfection ON/OFF, 6 - Setback ON/OFF, 7 - Defrost/startup ON/OFF, 8 - DHW reheat ON/OFF, 9 - DHW storage ON/OFF
  modbus_construct(&packets[PACKET4], 2, READ_INPUT_REGISTERS, 123, 1, 10);  // 10 - Leaving water temp
  modbus_construct(&packets[PACKET5], 2, READ_INPUT_REGISTERS, 131, 3, 11);  // 11 - Return water temp, 12 - DHW temp, 13 - Outdoor temp
  //READ UNIT CONTROL PARAMETERS
  modbus_construct(&packets[PACKET6], 2, READ_HOLDING_REGISTERS, 1, 1, 14);  // 14 - Leaving water setpoint (°C)
  modbus_construct(&packets[PACKET7], 2, READ_HOLDING_REGISTERS, 4, 4, 15);  // 15 - Modbus ON/OFF heating, 16 - Room Temp Setpoint, 17 - Modbus DHW reheat ON/OFF, 18 - Start DHW storage (Idle/Start)
  modbus_construct(&packets[PACKET8], 2, READ_HOLDING_REGISTERS, 9, 3, 19);    // 19 - Quiet mode, 20 - Weather dependent setpoint operation ON/OFF, 21 - Leaving water shift value during WDSO (-5..+5°C)
  //UNIT CONTROL
  modbus_construct(&packets[PACKET9], 2, PRESET_MULTIPLE_REGISTERS, 1, 1, 30);  // leaving water setpoint (25 - 80 °C)
  modbus_construct(&packets[PACKET10], 2, PRESET_MULTIPLE_REGISTERS, 4, 1, 31); // Heating 0/1
  modbus_construct(&packets[PACKET11], 2, PRESET_MULTIPLE_REGISTERS, 5, 1, 32); // Room temperature setpoint (16 - 32 °C)
  modbus_construct(&packets[PACKET12], 2, PRESET_MULTIPLE_REGISTERS, 6, 1, 33); // DHW reheat 0/1 (weather the unit will reheat water when its temp falls under setpoint)
  modbus_construct(&packets[PACKET13], 2, PRESET_MULTIPLE_REGISTERS, 7, 1, 34); // Start DHW storage (0:idle, 1:start now)
  modbus_construct(&packets[PACKET14], 2, PRESET_MULTIPLE_REGISTERS, 9, 1, 35); // Quiet mode operation (0:disable, 1:enable)
  modbus_construct(&packets[PACKET15], 2, PRESET_MULTIPLE_REGISTERS, 10, 1, 36); // Weather dependent setpoint operation (0:disable, 1:enable)
  modbus_construct(&packets[PACKET16], 2, PRESET_MULTIPLE_REGISTERS, 11, 1, 37); // Leaving water shift value during WDSO (-5..+5°C)

  packets[PACKET9].connection = 0; //do not send packet by default
  packets[PACKET10].connection = 0; //do not send packet by default
  packets[PACKET11].connection = 0; //do not send packet by default
  packets[PACKET12].connection = 0; //do not send packet by default
  packets[PACKET13].connection = 0; //do not send packet by default
  packets[PACKET14].connection = 0; //do not send packet by default
  packets[PACKET15].connection = 0; //do not send packet by default
  packets[PACKET16].connection = 0; //do not send packet by default

  // Initialize the Modbus Finite State Machine
  modbus_configure(&Serial, baud, SERIAL_8N1, timeout, polling, retry_count, TxEnablePin, packets, TOTAL_NO_OF_PACKETS, regs);

}

void setup_wifi() {

  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  if (strcmp(topic,setMQTTTopic("1", inTopic))==0){ // leaving water setpoint (25 - 80 °C)
    byte* p = (byte*)malloc(length);
    memcpy(p,payload,length);
    int tmp = atoi((char *) p);
    free(p);
    if(tmp >= 25 && tmp <= 80){
      regs[30] = tmp;
      packets[PACKET9].connection = 1;
    }
  }
  else if (strcmp(topic,setMQTTTopic("4", inTopic))==0){ // Heating 0/1
    byte* p = (byte*)malloc(length);
    memcpy(p,payload,length);
    int tmp = atoi((char *) p);
    free(p);
    if(tmp == 0){
      regs[31] = 0;
      packets[PACKET10].connection = 1;
    }
    else if(tmp == 1){
      regs[31] = 1;
      packets[PACKET10].connection = 1;
    }
  }
  else if (strcmp(topic,setMQTTTopic("5", inTopic))==0){ // Room temperature setpoint (16 - 32 °C)
    byte* p = (byte*)malloc(length);
    memcpy(p,payload,length);
    int tmp = atoi((char *) p);
    free(p);
    if(tmp >= 16 && tmp <= 32){
      regs[32] = tmp;
      packets[PACKET11].connection = 1;
    }
  }
  else if (strcmp(topic,setMQTTTopic("6", inTopic))==0){ // DHW reheat 0/1 (weather the unit will reheat water when its temp falls under setpoint)
    byte* p = (byte*)malloc(length);
    memcpy(p,payload,length);
    int tmp = atoi((char *) p);
    free(p);
    if(tmp == 0){
      regs[33] = 0;
      packets[PACKET12].connection = 1;
    }
    else if(tmp == 1){
      regs[33] = 1;
      packets[PACKET12].connection = 1;
    }
  }
  else if (strcmp(topic,setMQTTTopic("7", inTopic))==0){ // Start DHW storage (0:idle, 1:start now)
    byte* p = (byte*)malloc(length);
    memcpy(p,payload,length);
    int tmp = atoi((char *) p);
    free(p);
    if(tmp == 0){
      regs[34] = 0;
      packets[PACKET13].connection = 1;
    }
    else if(tmp == 1){
      regs[34] = 1;
      packets[PACKET13].connection = 1;
    }
  }
  else if (strcmp(topic,setMQTTTopic("9", inTopic))==0){ // Quiet mode operation (0:disable, 1:enable)
    byte* p = (byte*)malloc(length);
    memcpy(p,payload,length);
    int tmp = atoi((char *) p);
    free(p);
    if(tmp == 0){
      regs[35] = 0;
      packets[PACKET14].connection = 1;
    }
    else if(tmp == 1){
      regs[35] = 1;
      packets[PACKET14].connection = 1;
    }
  }
  else if (strcmp(topic,setMQTTTopic("10", inTopic))==0){ // Weather dependent setpoint operation (0:disable, 1:enable)
    byte* p = (byte*)malloc(length);
    memcpy(p,payload,length);
    int tmp = atoi((char *) p);
    free(p);
    if(tmp == 0){
      regs[36] = 0;
      packets[PACKET15].connection = 1;
    }
    else if(tmp == 1){
      regs[36] = 1;
      packets[PACKET15].connection = 1;
    }
  }
  else if (strcmp(topic,setMQTTTopic("11", inTopic))==0){ // Leaving water shift value during WDSO (-5..+5°C)
    byte* p = (byte*)malloc(length);
    memcpy(p,payload,length);
    int tmp = atoi((char *) p);
    free(p);
    if(tmp >= 0 && tmp <= 5){
      regs[37] = tmp;
      packets[PACKET16].connection = 1;
    }
    else if(tmp >= -5 && tmp < 0){
      regs[37] = tmp + 65536;
      packets[PACKET16].connection = 1;
    }
  }
  else if (strcmp(topic,setMQTTTopic("reboot_esp", inTopic))==0){ // Reboot
    byte* p = (byte*)malloc(length);
    memcpy(p,payload,length);
    int tmp = atoi((char *) p);
    free(p);
    if(tmp == 1){
      ESP.restart();
    }
  }
}



boolean reconnect() {
  if (client.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
    // Subscribe to input topic
    client.subscribe(setMQTTTopic("#", inTopic));
  }
  return client.connected();
}


void loop()
{
  
  modbus_update();

  if(packets[PACKET9].connection == 1 && packets[PACKET9].successful_requests > 0){    // leaving water setpoint (25 - 80 °C)
    client.publish(setMQTTTopic("update_1", outTopic), "true");
    packets[PACKET9].successful_requests = 0;
    packets[PACKET9].connection = 0;
  }
  if(packets[PACKET10].connection == 1 && packets[PACKET10].successful_requests > 0){  // Heating 0/1
    client.publish(setMQTTTopic("update_4", outTopic), "true");
    packets[PACKET10].successful_requests = 0;
    packets[PACKET10].connection = 0;
  }
  if(packets[PACKET11].connection == 1 && packets[PACKET11].successful_requests > 0){  // Room temperature setpoint (16 - 32 °C)
    client.publish(setMQTTTopic("update_5", outTopic), "true");
    packets[PACKET11].successful_requests = 0;
    packets[PACKET11].connection = 0;
  }
  if(packets[PACKET12].connection == 1 && packets[PACKET12].successful_requests > 0){  // DHW reheat 0/1 (weather the unit will reheat water when its temp falls under setpoint)
    client.publish(setMQTTTopic("update_6", outTopic), "true");
    packets[PACKET12].successful_requests = 0;
    packets[PACKET12].connection = 0;
  }
  if(packets[PACKET13].connection == 1 && packets[PACKET13].successful_requests > 0){  // Start DHW storage (0:idle, 1:start now)
    client.publish(setMQTTTopic("update_7", outTopic), "true");
    packets[PACKET13].successful_requests = 0;
    packets[PACKET13].connection = 0;
  }
  if(packets[PACKET14].connection == 1 && packets[PACKET14].successful_requests > 0){  // Quiet mode operation (0:disable, 1:enable)
    client.publish(setMQTTTopic("update_9", outTopic), "true");
    packets[PACKET14].successful_requests = 0;
    packets[PACKET14].connection = 0;
  }
  if(packets[PACKET15].connection == 1 && packets[PACKET15].successful_requests > 0){  // Weather dependent setpoint operation (0:disable, 1:enable)
    client.publish(setMQTTTopic("update_10", outTopic), "true");
    packets[PACKET15].successful_requests = 0;
    packets[PACKET15].connection = 0;
  }
  if(packets[PACKET16].connection == 1 && packets[PACKET16].successful_requests > 0){  // Leaving water shift value during WDSO (-5..+5°C)
    client.publish(setMQTTTopic("update_11", outTopic), "true");
    packets[PACKET16].successful_requests = 0;
    packets[PACKET16].connection = 0;
  }


  // Reconnect to MQTT broker if lost connection lost
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  char bufEmon[10];  // buffer to store data type conversion from float to string

  // Publish all readings to MQTT broker
    long now = millis();
    /* Publishing for EmonCMS - sent every 10 seconds */
    if (now - lastEmonCMSMsg > longInterval ) {
      lastEmonCMSMsg = now;
         
      itoa(regs[1], bufEmon, 10);                                     // 1 - Remcon Room temperature
      client.publish(setMQTTTopic("50", outTopic), bufEmon);

      itoa(regs[10], bufEmon, 10);                                    // 10 - Leaving water temp
      client.publish(setMQTTTopic("123", outTopic), bufEmon);

      itoa(regs[11], bufEmon, 10);                                    // 11 - Return water temp
      client.publish(setMQTTTopic("131", outTopic), bufEmon);

      itoa(regs[12], bufEmon, 10);                                    // 12 - DHW temp
      client.publish(setMQTTTopic("132", outTopic), bufEmon);

      if(regs[13] > 32767){
        itoa((regs[13] - 65536), bufEmon, 10);                        // 13 - Outdoor temp
      }
      else {
        itoa(regs[13], bufEmon, 10);                                  // 13 - Outdoor temp
      }
      client.publish(setMQTTTopic("133", outTopic), bufEmon);


      // PACKET ERRORS
      if(packets[PACKET1].failed_requests > 0){
        itoa(packets[PACKET1].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/1", outTopic), bufEmon);
        packets[PACKET1].failed_requests = 0;
      }
      if(packets[PACKET2].failed_requests > 0){
        itoa(packets[PACKET2].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/2", outTopic), bufEmon);
        packets[PACKET2].failed_requests = 0;
      }
      if(packets[PACKET3].failed_requests > 0){
        itoa(packets[PACKET3].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/3", outTopic), bufEmon);
        packets[PACKET3].failed_requests = 0;
      }
      if(packets[PACKET4].failed_requests > 0){
        itoa(packets[PACKET4].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/4", outTopic), bufEmon);
        packets[PACKET4].failed_requests = 0;
      }
      if(packets[PACKET5].failed_requests > 0){
        itoa(packets[PACKET5].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/5", outTopic), bufEmon);
        packets[PACKET5].failed_requests = 0;
      }
      if(packets[PACKET6].failed_requests > 0){
        itoa(packets[PACKET6].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/6", outTopic), bufEmon);
        packets[PACKET6].failed_requests = 0;
      }
      if(packets[PACKET7].failed_requests > 0){
        itoa(packets[PACKET7].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/7", outTopic), bufEmon);
        packets[PACKET7].failed_requests = 0;
      }
      if(packets[PACKET8].failed_requests > 0){
        itoa(packets[PACKET8].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/8", outTopic), bufEmon);
        packets[PACKET8].failed_requests = 0;
      }
      if(packets[PACKET9].failed_requests > 0){
        itoa(packets[PACKET9].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/9", outTopic), bufEmon);
        packets[PACKET9].failed_requests = 0;
      }
      if(packets[PACKET10].failed_requests > 0){
        itoa(packets[PACKET10].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/10", outTopic), bufEmon);
        packets[PACKET10].failed_requests = 0;
      }
      if(packets[PACKET11].failed_requests > 0){
        itoa(packets[PACKET11].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/11", outTopic), bufEmon);
        packets[PACKET11].failed_requests = 0;
      }
      if(packets[PACKET12].failed_requests > 0){
        itoa(packets[PACKET12].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/12", outTopic), bufEmon);
        packets[PACKET12].failed_requests = 0;
      }
      if(packets[PACKET13].failed_requests > 0){
        itoa(packets[PACKET13].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/13", outTopic), bufEmon);
        packets[PACKET13].failed_requests = 0;
      }
      if(packets[PACKET14].failed_requests > 0){
        itoa(packets[PACKET14].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/14", outTopic), bufEmon);
        packets[PACKET14].failed_requests = 0;
      }
      if(packets[PACKET15].failed_requests > 0){
        itoa(packets[PACKET15].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/15", outTopic), bufEmon);
        packets[PACKET15].failed_requests = 0;
      }
      if(packets[PACKET16].failed_requests > 0){
        itoa(packets[PACKET16].failed_requests, bufEmon, 10);
        client.publish(setMQTTTopic("packet_failed/16", outTopic), bufEmon);
        packets[PACKET16].failed_requests = 0;
      }
    }

    if (now - lastMsg > shortInterval) {
      lastMsg = now;

      if(regs[2] != regs_p[2]){                                         // 2 - Space heating ON/OFF
        regs_p[2] = regs[2];
        itoa(regs[2], bufEmon, 10);
        client.publish(setMQTTTopic("70", outTopic), bufEmon, true);
        if(regs[2] == 0){
          regs[31] = 0;
          packets[PACKET10].connection = 1;
        }
        else if(regs[2] == 1){
          regs[31] = 1;
          packets[PACKET10].connection = 1;
        }
      }
      if(regs[3] != regs_p[3]){                                         // 3 - Circulation pump ON/OFF
        regs_p[3] = regs[3];
        itoa(regs[3], bufEmon, 10);
          client.publish(setMQTTTopic("71", outTopic), bufEmon, true);
      }
      if(regs[4] != regs_p[4]){                                         // 4 - Compressor ON/OFF
        regs_p[4] = regs[4];
        itoa(regs[4], bufEmon, 10);                                     
        client.publish(setMQTTTopic("72", outTopic), bufEmon, true);
      }
      if(regs[5] != regs_p[5]){                                         // 5 - Disinfection ON/OFF
        regs_p[5] = regs[5];
        itoa(regs[5], bufEmon, 10);                                     
        client.publish(setMQTTTopic("74", outTopic), bufEmon, true);
      }
      if(regs[6] != regs_p[6]){                                         // 6 - Setback ON/OFF
        regs_p[6] = regs[6];
        itoa(regs[6], bufEmon, 10);    
        client.publish(setMQTTTopic("75", outTopic), bufEmon, true);
      }
      if(regs[7] != regs_p[7]){                                         // 7 - Defrost/startup ON/OFF
        regs_p[7] = regs[7];
        itoa(regs[7], bufEmon, 10);                                     
        client.publish(setMQTTTopic("76", outTopic), bufEmon, true);
      }
      if(regs[8] != regs_p[8]){                                         // 8 - DHW reheat ON/OFF
        regs_p[8] = regs[8];
        itoa(regs[8], bufEmon, 10);      
        client.publish(setMQTTTopic("77", outTopic), bufEmon, true);
        if(regs[8] == 0){
          regs[33] = 0;
          packets[PACKET12].connection = 1;
        }
        else if(regs[8] == 1){
          regs[33] = 1;
          packets[PACKET12].connection = 1;
        }
      }
      if(regs[9] != regs_p[9]){                                         // 9 - DHW storage ON/OFF
        regs_p[9] = regs[9];
        itoa(regs[9], bufEmon, 10);          
        client.publish(setMQTTTopic("78", outTopic), bufEmon, true);
      }
      if(regs[14] != regs_p[14]){                                       // 14 - Leaving water setpoint (°C)
        regs_p[14] = regs[14];
        itoa(regs[14], bufEmon, 10);
        client.publish(setMQTTTopic("1", outTopic), bufEmon, true);
      }
      if(regs[15] != regs_p[15]){                                       // 15 - Modbus ON/OFF heating
        regs_p[15] = regs[15];
        itoa(regs[15], bufEmon, 10);
        client.publish(setMQTTTopic("4", outTopic), bufEmon, true);
      }
      if(regs[16] != regs_p[16]){                                       // 16 - Room Temp Setpoint
        regs_p[16] = regs[16];
        itoa(regs[16], bufEmon, 10);
        client.publish(setMQTTTopic("5", outTopic), bufEmon, true);
      }
      if(regs[17] != regs_p[17]){                                       // 17 - Modbus DHW reheat ON/OFF
        regs_p[17] = regs[17];
        itoa(regs[17], bufEmon, 10);
        client.publish(setMQTTTopic("6", outTopic), bufEmon, true);
      }
      if(regs[18] != regs_p[18]){                                       // 18 - Start DHW storage (Idle/Start)
        regs_p[18] = regs[18];
        itoa(regs[18], bufEmon, 10);
        client.publish(setMQTTTopic("7", outTopic), bufEmon, true);
      }
      if(regs[19] != regs_p[19]){                                       // 19 - Quiet mode ON/OFF
        regs_p[19] = regs[19];
        itoa(regs[19], bufEmon, 10);
        client.publish(setMQTTTopic("9", outTopic), bufEmon, true);
      }
      if(regs[20] != regs_p[20]){                                       // 20 - Weather dependent setpoint operation ON/OFF
        regs_p[20] = regs[20];
        itoa(regs[20], bufEmon, 10);
        client.publish(setMQTTTopic("10", outTopic), bufEmon, true);
      }
      if(regs[21] != regs_p[21]){                                       // 21 - Leaving water shift value during WDSO (-5..+5°C)
        regs_p[21] = regs[21];
        if(regs[21] > 32767){
          itoa((regs[21] - 65536), bufEmon, 10);
        }
        else {
          itoa(regs[21], bufEmon, 10);
        }
        client.publish(setMQTTTopic("11", outTopic), bufEmon, true);
      }
     
    }

}


char* setMQTTTopic(char* var, String topicBase){
  String myBigArray = "";
  char message_buff[150];
  myBigArray = topicBase + var;
  myBigArray.toCharArray(message_buff, myBigArray.length()+1); 
  return message_buff;
}

float reform_uint16_2_float32(uint16_t u1, uint16_t u2)
{  
   uint32_t num = ((uint32_t)u1 & 0xFFFF) << 16 | ((uint32_t)u2 & 0xFFFF);
    float numf;
    memcpy(&numf, &num, 4);
    return numf;
}

