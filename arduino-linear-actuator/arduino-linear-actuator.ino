// see https://microcontrollerslab.com/esp32-esp8266-momentary-switch-web-server-control-gpio-outputs/

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#else
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

#include <ElegantOTA.h>
#include <ESPAsyncWiFiManager.h>
#include <Ticker.h>
#include <EEPROM.h>

#include "index.h"
#include "secrets.h"

// PINOUT
// https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
const int ONBOARD_LED = 2;
const int PWMA = 13;  // D7
const int AIN2 = 12;  // D6
const int AIN1 = 14;  // D5
//const int STBY = 10;
// const int RED = 2;
// const int BLUE = 3;
const int YELLOW = 4;  // D2
const int GREEN = 5;   // D1
const int TEMP_IN = A0;
//const int POT_PIN = A15;  // ESP8266 only has one analog input

// CONSTANTS
const bool RETRACT = true;
const bool EXTEND = false;
const String version = "v0.1.0";

enum class States { STOPPED, WINDOW_OPEN, WINDOW_CLOSED, RETRACTING, EXTENDING, TIMEOUT, ERROR };
States state = States::STOPPED;
States prev_state = States::STOPPED;

// DEFAULTS
bool moving = false;
bool direction = RETRACT;
bool lower_limit = false;
bool upper_limit = false;
bool prev_lower_limit = lower_limit;
bool prev_upper_limit = upper_limit;
float default_temp = -100.0;
float degC = default_temp;
float degF = default_temp;
float default_off_degF = 85.0;
float default_on_degF = 76.0;
float scale = 1.0;
float offset = 0.0;

bool automatic = true;

// PARAMETERS
unsigned long temp_sample_period = 1000;  // ms
unsigned long last_changed = 0;
unsigned long motor_timeout = 30000;  // ms

struct { 
  float off_degF = default_off_degF;
  float on_degF = default_on_degF;
} parameters;
uint addr = 0;

// WEB SETUP
AsyncWebServer server(80);
AsyncEventSource events("/events");
DNSServer dns;

// TICKER
void toggle_led();
Ticker ticker;

void setup() {
  Serial.begin(115200);

  /********************************
  // PINOUT SETUPS
  ********************************/
  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, HIGH);  // turn on LED after WiFi connects

  pinMode(PWMA, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(AIN1, OUTPUT);
  //pinMode(STBY, OUTPUT);  // unused

  pinMode(YELLOW, INPUT_PULLUP);
  pinMode(GREEN, INPUT_PULLUP);

  pinMode(TEMP_IN, INPUT);

  /********************************
  // WIFI CONFIG
  ********************************/
  AsyncWiFiManager wifiManager(&server,&dns);

  // reset stored WiFi credentials if both buttons are pressed on boot-up
  if ( !digitalRead(YELLOW) && !digitalRead(GREEN) ) { // reset stored WiFi credentials if both buttons are pressed on boot-up
    wifiManager.resetSettings();
  }

  String ssid = "AP_";
  ssid += String(ESP.getChipId());
  String password = "greenhouse";

  wifiManager.setAPCallback(configModeCallback);
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(180);

  if (wifiManager.autoConnect(ssid.c_str(), password.c_str())) {
    Serial.println("CONNECTED!");
    ticker.detach();  // stop blinking
    digitalWrite(ONBOARD_LED, LOW);  // turn on LED after WiFi connects
  }
  else {
    Serial.println("FAILED TO CONNECT");
    ticker.attach(0.1, toggle_led);
    // delay(3000);
    // //reset and try again, or maybe put it to deep sleep
    // ESP.reset();
    // delay(5000);
  }

  /********************************
  // MEMORY
  ********************************/
  EEPROM.begin(512);
  EEPROM.get(addr,parameters);

  /********************************
  // SERVER CALLBACKS
  ********************************/
  // Send web page to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  // Receive an HTTP GET request
  server.on("/ON", HTTP_GET, [](AsyncWebServerRequest *request) {
    digitalWrite(ONBOARD_LED, HIGH);
    request->send(200, "text/plain", "OK");
  });

  // Receive an HTTP GET request
  server.on("/OFF", HTTP_GET, [](AsyncWebServerRequest *request) {
    digitalWrite(ONBOARD_LED, LOW);
    request->send(200, "text/plain", "OK");
  });

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    //request->send_P(200, "text/plain", String(degC).c_str());
    request->send_P(200, "text/plain", format_float(degF).c_str());
  });

  server.on("/command", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool ok = false;
    //Serial.print("/command");

    if ( request->hasParam("type") ){
      String type = request->getParam("type")->value();
      Serial.print("?type=");
      Serial.print(type);

      if ( type.equals("extend") ){
        extend();
        ok = true;
      }
      else if ( type.equals("retract") ){
        retract();
        ok = true;
      }
      else if ( type.equals("stop") ){
        stop();
        automatic = false;
        ok = true;
      }

      Serial.println();
    } else {
      Serial.println("NONE");
    }

    if (ok){
      request->send(200, "text/plain", "OK");
    }
    else {
      request->send(200, "text/plain", "FAIL");  // TODO: How do I handle off-nominal return codes?
    }
    
  });

  server.on("/setpoints", HTTP_GET, [](AsyncWebServerRequest *request) {
    String temp_off = request->getParam("off")->value();
    String temp_on = request->getParam("on")->value();
    Serial.println("Off=" + temp_off + ", On=" + temp_on);
    
    parameters.off_degF = temp_off.toFloat();
    parameters.on_degF = temp_on.toFloat();
    if (parameters.off_degF > parameters.on_degF){
      request->send(200, "text/plain", "OK");
    }
    else {
      parameters.off_degF = default_off_degF;
      parameters.on_degF = default_on_degF;
      request->send(200, "text/plain", "FAIL");  // TODO: How do I handle off-nominal return codes?
    }

    EEPROM.put(addr, parameters);
    EEPROM.commit();
  });

  server.on("/checkbox", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool ok = false;
    //Serial.print("/command");

    if ( request->hasParam("output") ){
      String output = request->getParam("output")->value();
      if ( output.equals("automatic") ){
        ok = true;
        String state = request->getParam("state")->value();
        if (state.toInt() == 1){
          automatic = true;
        }
        else {
          automatic = false;
          stop();
        }
      }
    }

    if (ok){
      request->send(200, "text/plain", "OK");
    }
    else {
      request->send(200, "text/plain", "FAIL");  // TODO: How do I handle off-nominal return codes?
    }
  });

  /********************************
  // Webserver Events
  ********************************/
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    events.send(state_string(state).c_str(), "state", millis());
  });
  server.addHandler(&events);

  ElegantOTA.begin(&server);
  server.begin();
}

void loop() {
  check_limits();

  static unsigned long prev_millis = 0;
  if (millis() - prev_millis > temp_sample_period) {
    update_temps();

    if (automatic){
      if (degF < parameters.on_degF && !lower_limit){
        retract();
      } 
      else if (degF > parameters.off_degF && !upper_limit){
        extend();
      }
    }

    // DEBUG
    Serial.println(degC);
    prev_millis = millis();
  }

  if (state == prev_state) {
    if ((state==States::RETRACTING || state==States::EXTENDING) && (millis() - last_changed > motor_timeout)){
      stop();
      state = States::TIMEOUT;
      automatic = false;
      Serial.println("TIMEOUT");
    }
  }
  if (state != prev_state) {
    last_changed = millis();

    events.send(state_string(state).c_str(), "state", millis());
    
    Serial.print("State changed from ");
    Serial.print(state_string(prev_state));
    Serial.print(" to ");
    Serial.println(state_string(state));
  } 
  prev_state = state;

  analogWrite(PWMA, 255); // TODO: make this programmable..

  ElegantOTA.loop();
}

// Replaces placeholder with button section in your web page
String processor(const String &var) {
  //Serial.println(var);
  if (var == "TEMPERATURE") {
    return format_float(degF);
  }
  else if (var == "UPPER_LIMIT"){
    return String(upper_limit).c_str();
  }
  else if (var == "LOWER_LIMIT"){
    return String(lower_limit).c_str();
  }
  else if (var == "TURN_OFF"){
    return String(parameters.off_degF).c_str();
  }
  else if (var == "TURN_ON"){
    return String(parameters.on_degF).c_str();
  }  
  else if (var == "VERSION"){
    return version.c_str();
  }
  else if (var == "STATE"){
    return state_string(state).c_str();
  }
  else if (var == "AUTOMATIC_CHECKED"){
    return (automatic) ? "checked" : "";
  }
  return String();
}

void update_temps() {
  float voltage_ref = 3.3;
  int analog_in = analogRead(TEMP_IN);
  //degC = 25.0 + ((analog_in / 1023) * 5.0 - .750)/0.010;
  float volts = ((float)analog_in / 1023.0) * voltage_ref;  // TODO: change to 3.3V
  float new_degC = 25.0 + (volts - .750) / 0.010;           // 25C at 750mV, with 1 degC/10 mV
  if (degC == default_temp) {
    degC = new_degC;
  }
  degC = moving_average(new_degC, degC, 10);
  degF = CtoF(degC);
}

float CtoF(float C){
  return (C * 9.0 / 5.0) + 32.0;
}

String format_float(float f) {
  char buffer[80];
  sprintf(buffer, "%.1f", f);
  return String(buffer);
}

void check_limits() {
  lower_limit = !digitalRead(YELLOW);  // true if at LOWER limit
  upper_limit = !digitalRead(GREEN);   // true if at upper limit

  if (!moving && state!=States::TIMEOUT){
    state = States::STOPPED;
  }

  if (lower_limit){
    //if (moving && direction == RETRACT) {
    stop();
    state = States::WINDOW_CLOSED;
  } 

  if (upper_limit){
    //if (moving && direction == EXTEND) {
    stop();
    state = States::WINDOW_OPEN;
  }

  if (lower_limit && upper_limit){
    state = States::ERROR;
  }

  // if (lower_limit != prev_lower_limit ){
  //   //events.send(String(lower_limit).c_str(), "lower_limit", millis());
  // }

  // if (upper_limit != prev_upper_limit ){
  //   //events.send(String(upper_limit).c_str(), "upper_limit", millis());
  // }

  prev_lower_limit = lower_limit;
  prev_upper_limit = upper_limit;
}

String state_string(States s){
  switch (s) {
    case States::STOPPED:
      return "STOPPED";
      break;
    case States::WINDOW_OPEN:
      return "WINDOW_OPEN";
      break;
    case States::WINDOW_CLOSED:
      return "WINDOW_CLOSED";
      break;
    case States::EXTENDING:
      return "EXTENDING";
      break;
    case States::RETRACTING:
      return "RETRACTING";
      break;
    case States::ERROR:
      return "ERROR";
      break;
    case States::TIMEOUT:
      return "TIMEOUT";
      break;
    default:
      return "UNKNOWN";
      break;
  }
}

void stop() {
  moving = false;
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  Serial.println("STOP");
  //events.send("stop", "stop", millis());
  //state = STOPPED;
}

void extend() {
  direction = EXTEND;
  go();
  Serial.println("EXTEND");
  //events.send("extend", "extend", millis());
  state = States::EXTENDING;
}

void retract() {
  direction = RETRACT;
  go();
  Serial.println("RETRACT");
  //events.send("retract", "retract", millis());
  state = States::RETRACTING;
}

void go() {
  moving = true;
  if (direction == RETRACT) {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
  } else {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  }
}

float moving_average(float new_sample, float last_average, int N) {
  // Credit to u/stockvu https://www.reddit.com/r/arduino/comments/w10nr8/ignore_the_10_first_readings_of_the_sensor/igjg95z/
  //static float last_average = new_sample;
  float partial_1 = (1 / (float)N) * new_sample;
  float partial_2 = ((N - 1) / (float)N) * last_average;
  last_average = partial_1 + partial_2;
  return last_average;
}

void toggle_led()
{
  //toggle state
  int state = digitalRead(ONBOARD_LED);  // get the current state of GPIO1 pin
  digitalWrite(ONBOARD_LED, !state);     // set pin to the opposite state
}

void configModeCallback(AsyncWiFiManager *myWiFiManager){
  Serial.println("ENTERING CONFIG MODE...");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.5, toggle_led);
}