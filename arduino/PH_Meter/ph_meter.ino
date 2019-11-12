#include <ESP8266WiFi.h> 
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <string.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"

const char* ssid = "Your SSID";
const char* password = "Your Wifi Password";
const char* mqttServer = "Your MQTT Server IP";
const char* mqttUserName = "Your MQTT Server Username";
const char* mqttPwd = "Your MQTT Server Password";
const char* clientID = "nodemcu";      // any id
const char* DeviceId = "dev1";
const char* DataTopic = "data/dev1";
const char* EventTopic = "event/dev1";
const char* CommandTopic = "cmd/dev1";
const char* CommandResponseTopic = "cmd_resp/dev1";

#define SensorPin A0            //pH meter Analog output to Arduino Analog Input 2
#define LED BUILTIN_LED
#define samplingInterval 20
#define printInterval 800
#define ArrayLenth  40    //times of collection
#define DQ_pin 2 //D4  

OneWire oneWire(DQ_pin);
DallasTemperature temp_sensors(&oneWire);
double temp_value;


int pHArray[ArrayLenth];   //Store the average value of the sensor feedback
int pHArrayIndex=0;    

const char* cmd_ph7cal = "cmd_ph7cal";
const char* cmd_ph10cal = "cmd_ph10cal";
const char* cmd_getstate = "cmd_getstate";
const char* cmd_reset = "cmd_reset";

unsigned long prevMillis = 0;
const long interval = 20000;  // upload data interval, 20s。

enum State {
  Normal,
  UnCalibrated,
  PH7Caling,
  PH7Caled,
  PH10Caling
  //PH10Caled, eq to NORMAL or UnCalibrated
};

State state = UnCalibrated;

double ph_value;   // PH
double ph_slope;   // PHValue = slope*voltage+Offset;
double ph_offset;   // PHValue = slope*voltage+Offset;
const double ph_slope_default =0.007247947;
const double ph_offset_default=2.699647957;
float voltage7;
float voltage10;


WiFiClient espClient;
PubSubClient mqttClient(espClient);

WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP);
String now = "";

//--------------- Wifi
void setup_wifi() {
  delay(10);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//--------------- MQTT
void mqttReconnect() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientID, mqttUserName, mqttPwd)) {
      Serial.println("MQTT connected");
      mqttClient.subscribe(CommandTopic);
      mqttClient.setCallback(mqttCallback);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000); 
    }
  }
}

//--------------- Configuration
bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, buf.get());
  
  if (doc.isNull()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  ph_slope = doc["ph_slope"];
  ph_offset = doc["ph_offset"];

  // Real world application would store these values in some variables for
  // later use.

  Serial.print("PH slope: ");
  Serial.print(ph_slope,10);
  Serial.print(" PH offset:");
  Serial.println(ph_offset,10);
  return true;
}

bool saveConfig(double ph_slope_input,double ph_offset_input) {
  StaticJsonDocument<256> doc;
  doc["ph_slope"] = ph_slope_input;
  doc["ph_offset"] = ph_offset_input;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  // Serialize JSON to file
  if (serializeJson(doc, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  return true;
}
// PH Reading
double avergearray(int* arr, int number){
  int i;
  int max,min;
  double avg;
  long amount=0;
  if(number<=0){
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if(number<5){   //less than 5, calculated directly statistics
    for(i=0;i<number;i++){
      amount+=arr[i];
    }
    avg = amount/number;
    return avg;
  }else{
    if(arr[0]<arr[1]){
      min = arr[0];max=arr[1];
    }
    else{
      min=arr[1];max=arr[0];
    }
    for(i=2;i<number;i++){
      if(arr[i]<min){
        amount+=min;        //arr<min
        min=arr[i];
      }else {
        if(arr[i]>max){
          amount+=max;    //arr>max
          max=arr[i];
        }else{
          amount+=arr[i]; //min<=arr<=max
        }
      }//if
    }//for
    avg = (double)amount/(number-2);
  }//if
  return avg;
}


//--------------- Begin of code

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output pinMode(BUILTIN_LED, OUTPUT); 
  Serial.begin(115200);
  setup_wifi();
  mqttClient.setServer(mqttServer, 1883);
  timeClient.begin();
  timeClient.setTimeOffset(28800);//+8 hr
  timeClient.update();

  Serial.println("");
  delay(1000);
  Serial.println("Mounting FS...");
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  if (!loadConfig()) {
    Serial.println("Failed to load config");
    change_state(UnCalibrated);
  } else {
    Serial.println("Config loaded");
    if (ph_slope==0.0&&ph_offset==0.0) {
      Serial.println("ph_slope and ph_offset have no value on configuration");
      change_state(UnCalibrated);
    }else{
      change_state(Normal);
    }
  }

  temp_sensors.begin();
  
}

void loop() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  switch (state){
   case Normal:
      normal_loop();
      break;
    case UnCalibrated:
      uncalibrated_loop();
      break;
    case PH7Caling:
      ph7caling_loop();
      break;
    case PH7Caled:
      ph7caled_loop();
      break;
    case PH10Caling:
      ph10caling_loop();
      break;
    default:
      // statements
      break;
  }
}


void normal_loop() {
  // SamplingInterval = 20ms
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();
  static float voltage;
  if(millis()-samplingTime > samplingInterval)
  {
      pHArray[pHArrayIndex++]=analogRead(SensorPin);
      if(pHArrayIndex==ArrayLenth)pHArrayIndex=0;
      //voltage = avergearray(pHArray, ArrayLenth)*5.0/1024;
      //pHValue = 3.5*voltage+Offset;
      voltage = avergearray(pHArray, ArrayLenth);
      ph_value = ph_slope*voltage+ph_offset;

      samplingTime=millis();
  }
  
  if(millis() - printTime > printInterval)   //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
  {
        Serial.print("Voltage:");
        Serial.print(voltage,2);
        Serial.print("    pH value: ");
        Serial.println(ph_value,2);
        digitalWrite(LED,digitalRead(LED)^1);
        printTime=millis();
  }
  
  // Every 20 Second, Upload Data
  if (millis() - prevMillis > interval) {
    prevMillis = millis();

    // Read temperature
    temp_sensors.requestTemperatures();
    temp_value = temp_sensors.getTempCByIndex(0);    

    // Create MQTT Payload
    String msgStr = "";
    msgStr=msgStr+"{ \"id\":\""+DeviceId+"\", \"temp\":"+temp_value+", \"ph\":"+ph_value+", \"voltage\":"+voltage+"}";
    mqtt_publish(DataTopic, msgStr);
    msgStr = "";
  }
}

void uncalibrated_loop() {
  if (millis() - prevMillis > interval) {
    prevMillis = millis();
    
    now = timeClient.getFormattedTime();
    Serial.print(now);
    
    Serial.println(" State: UnCalibrated");

    if (saveConfig(ph_slope_default,ph_offset_default)){
      ph_slope = ph_slope_default;
      ph_offset = ph_offset_default;
      change_state(Normal);
    }
  }
}

void ph7caling_loop() {
  // SamplingInterval = 20ms
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();
  
  if(millis()-samplingTime > samplingInterval)
  {
      pHArray[pHArrayIndex++]=analogRead(SensorPin);
      if(pHArrayIndex==ArrayLenth)pHArrayIndex=0;
      voltage7 = avergearray(pHArray, ArrayLenth); 
      samplingTime=millis();
  }
  
  if(millis() - printTime > printInterval)   //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
  {
        Serial.print("Calibrate PH7, Get Voltage:");
        Serial.println(voltage7,2);
        printTime=millis();
        change_state(PH7Caled);
  }
}
void ph7caled_loop() {
  for (int i=0;i<6;i++){
    digitalWrite(LED,digitalRead(LED)^1);
    delay(100);
  }
  if (millis() - prevMillis > interval) {
    prevMillis = millis();
    Serial.println("State: PH7Caled");
  }
}
void ph10caling_loop() {
  // SamplingInterval = 20ms
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();
  
  if(millis()-samplingTime > samplingInterval)
  {
      pHArray[pHArrayIndex++]=analogRead(SensorPin);
      if(pHArrayIndex==ArrayLenth)pHArrayIndex=0;
      voltage10 = avergearray(pHArray, ArrayLenth); 
      samplingTime=millis();
  }
  
  if(millis() - printTime > printInterval)   //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
  {
        Serial.print("Calibrate PH10, Get Voltage:");
        Serial.println(voltage10,2);
        printTime=millis();

        PHCalibration();
        saveConfig(ph_slope,ph_offset);
        Serial.print("Calibration Completed: ph_slope=");
        Serial.print(ph_slope);
        Serial.print(" ph_offset=");
        Serial.print(ph_offset);
        Serial.println();
        change_state(Normal);
  }
}

void PHCalibration(){
  //voltage7, voltage10
  // S = (10.01-7) / (V2-V1)
  // Offset =  7 - S*V1
  ph_slope = 3.01 /(voltage10-voltage7);
  ph_offset =  7 - ph_slope*voltage7;
  
  now = timeClient.getFormattedTime();
  String msgStr = "";
  msgStr=msgStr+"{ \"id\":\""+DeviceId+"\", \"ts\":\""+now+"\", \"voltage7\":"+voltage7+", \"voltage10\":"+voltage10+", \"ph_slope\":\""+ph_slope+"\", \"ph_offset\":\""+ph_offset+"\"}";
  mqtt_publish(EventTopic, msgStr);
  msgStr="";
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char cmd[20];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    cmd[i]=(char)payload[i];
  }
  Serial.println();

  Serial.print("cmd is ");
  Serial.print(cmd);
  Serial.println();
  
  if(strncmp(cmd,cmd_ph7cal,10)==0){
    do_cmd_ph7cal();
  }else if(strncmp(cmd,cmd_ph10cal,11)==0){
    do_cmd_ph10cal();
  }else if(strncmp(cmd,cmd_getstate,12)==0){
    do_cmd_getstate();
  }else if(strncmp(cmd,cmd_reset,9)==0){
    do_cmd_reset();
  }

}

void do_cmd_ph7cal(){
  Serial.println("do_cmd_ph7cal()"); 
  change_state(PH7Caling);
}
void do_cmd_ph10cal(){
  Serial.println("do_cmd_ph10cal()"); 
  change_state(PH10Caling);
}
void do_cmd_getstate(){
    Serial.println("do_cmd_getstate()"); 
    publish_state();
}

void publish_state(){
    now = timeClient.getFormattedTime();
    String msgStr = "";
    msgStr=msgStr+"{ \"id\":\""+DeviceId+"\", \"ts\":\""+now+"\", \"state\":"+state+"}";
    mqtt_publish(EventTopic, msgStr);
}

void change_state(State s){
    state = s;
    publish_state();
}

void mqtt_publish(const char* topic, String str){
    byte arrSize = str.length() + 1;
    char msg[arrSize];
    Serial.print("Publish topic: ");
    Serial.print(topic);
    Serial.print(" message: ");
    Serial.print(str);
    Serial.print(" arrSize: ");
    Serial.println(arrSize);
    str.toCharArray(msg, arrSize);
    if (!mqttClient.publish(topic, msg)){
      Serial.println("Faliure to publish, maybe you should check the message size: MQTT_MAX_PACKET_SIZE 128");
    }
}


void(* resetFunc) (void) = 0;//declare reset function at address 0
void do_cmd_reset(){
  if (saveConfig(ph_slope_default,ph_offset_default)){
      resetFunc(); //call reset 
  }
}
