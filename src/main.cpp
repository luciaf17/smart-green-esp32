//VERY IMPORTANT
//to see colors in terminals add this line at the end of platformio.ini
//monitor_flags = --raw
#include <Arduino.h>
#include "IoTicosSplitter.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

String dId = "212121";
String webhook_pass = "24NbZkoZhI";
String webhook_endpoint = "http://192.168.1.108:3001/api/getdevicecredentials";
const char *mqtt_server = "192.168.1.108";

//PINS
#define led 2

//WiFi
const char *wifi_ssid = "Copca Ezequiel";
const char *wifi_password = "50551989";

//Functions definitions
bool get_mqtt_credentials();
void check_mqtt_connection();
bool reconnect();
void process_sensors();
void process_actuators();
void send_data_to_broker();
void callback(char *topic, byte *payload, unsigned int length);
void process_incoming_msg(String topic, String incoming);
void clear();

//Global Vars
WiFiClient espclient;
PubSubClient client(espclient);
IoTicosSplitter splitter;
long lastReconnectAttemp = 0;
long varsLastSend[20];

DynamicJsonDocument mqtt_data_doc(2048);

void setup()
{

  Serial.begin(921600);
  pinMode(led, OUTPUT);
  clear();

  Serial.print("\n\n\nWiFi Connection in Progress");

  WiFi.begin(wifi_ssid, wifi_password);

  int counter = 0;

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    counter++;

    if (counter > 10)
    {
      Serial.print("\n\n         Ups WiFi Connection Failed :( ");
      Serial.println(" -> Restarting...");
      delay(2000);
      ESP.restart();
    }
  }


  //Printing local ip
  Serial.println("\n\n         WiFi Connection -> SUCCESS :)");
  Serial.print("\n         Local IP -> ");
  Serial.print(WiFi.localIP());

  client.setCallback(callback);
}

void loop()
{
  check_mqtt_connection();
}

int prev_temp = 0;
int prev_hum = 0;
int prev_co2 = 0;

void process_sensors()
{

  //get temp simulation
  int temp = random(1, 100);
  mqtt_data_doc["variables"][0]["last"]["value"] = temp;

  //save temp?
  int dif = temp - prev_temp;
  if (dif < 0)
  {
    dif *= -1;
  }

  if (dif >= 40)
  {
    mqtt_data_doc["variables"][0]["last"]["save"] = 1;
  }
  else
  {
    mqtt_data_doc["variables"][0]["last"]["save"] = 0;
  }

  prev_temp = temp;

  //get humidity simulation
  int hum = random(1, 50);
  mqtt_data_doc["variables"][1]["last"]["value"] = hum;

  //save hum?
  dif = hum - prev_hum;
  if (dif < 0)
  {
    dif *= -1;
  }

  if (dif >= 20)
  {
    mqtt_data_doc["variables"][1]["last"]["save"] = 1;
  }
  else
  {
    mqtt_data_doc["variables"][1]["last"]["save"] = 0;
  }

  prev_hum = hum;

  //get c02 simulation
  int co2 = random(1, 50);
  mqtt_data_doc["variables"][5]["last"]["value"] = co2;

  //save hum?
  dif = co2 - prev_co2;
  if (dif < 0)
  {
    dif *= -1;
  }

  if (dif >= 20)
  {
    mqtt_data_doc["variables"][5]["last"]["save"] = 1;
  }
  else
  {
    mqtt_data_doc["variables"][5]["last"]["save"] = 0;
  }

  prev_hum = hum;

  //get led status
  mqtt_data_doc["variables"][4]["last"]["value"] = (HIGH == digitalRead(led));
}

void process_actuators()
{
  if (mqtt_data_doc["variables"][2]["last"]["value"] == "true")
  {
    digitalWrite(led, HIGH);
    mqtt_data_doc["variables"][2]["last"]["value"] = "";
    varsLastSend[4] = 0;
  }
  else if (mqtt_data_doc["variables"][3]["last"]["value"] == "false")
  {
    digitalWrite(led, LOW);
    mqtt_data_doc["variables"][3]["last"]["value"] = "";
    varsLastSend[4] = 0;
  }

}

String last_received_msg = "";
String last_received_topic = "";




//TEMPLATE ⤵
void process_incoming_msg(String topic, String incoming){

  last_received_topic = topic;
  last_received_msg = incoming;

  String variable = splitter.split(topic, '/', 2);

  for (int i = 0; i < mqtt_data_doc["variables"].size(); i++ ){

    if (mqtt_data_doc["variables"][i]["variable"] == variable){
      
      DynamicJsonDocument doc(256);
      deserializeJson(doc, incoming);
      mqtt_data_doc["variables"][i]["last"] = doc;

    }

  }

  process_actuators();

}

void callback(char *topic, byte *payload, unsigned int length)
{

  String incoming = "";

  for (int i = 0; i < length; i++)
  {
    incoming += (char)payload[i];
  }

  incoming.trim();

  process_incoming_msg(String(topic), incoming);

}



void send_data_to_broker()
{

  long now = millis();

  for (int i = 0; i < mqtt_data_doc["variables"].size(); i++)
  {

    if (mqtt_data_doc["variables"][i]["variableType"] == "output")
    {
      continue;
    }

    int freq = mqtt_data_doc["variables"][i]["variableSendFreq"];

    if (now - varsLastSend[i] > freq * 1000)
    {
      varsLastSend[i] = millis();

      String str_root_topic = mqtt_data_doc["topic"];
      String str_variable = mqtt_data_doc["variables"][i]["variable"];
      String topic = str_root_topic + str_variable + "/sdata";

      String toSend = "";

      serializeJson(mqtt_data_doc["variables"][i]["last"], toSend);

      client.publish(topic.c_str(), toSend.c_str());

      Serial.println(topic);
      Serial.println(toSend);
    }
  }
}

bool reconnect()
{

  if (!get_mqtt_credentials())
  {
    Serial.println("\n\n      Error getting mqtt credentials :( \n\n RESTARTING IN 10 SECONDS");
    delay(10000);
    ESP.restart();
  }

  //Setting up Mqtt Server
  client.setServer(mqtt_server, 1883);

  Serial.print("\n\n\nTrying MQTT Connection");

  String str_client_id = "device_" + dId + "_" + random(1, 9999);
  const char *username = mqtt_data_doc["username"];
  const char *password = mqtt_data_doc["password"];
  String str_topic = mqtt_data_doc["topic"];

  if (client.connect(str_client_id.c_str(), username, password))
  {
    Serial.print("\n\n         Mqtt Client Connected :) ");
    delay(2000);

    client.subscribe((str_topic + "+/actdata").c_str());
  }
  else
  {
    Serial.print("\n\n         Mqtt Client Connection Failed :( ");
  }
  return true;
}

void check_mqtt_connection()
{

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("\n\n         Ups WiFi Connection Failed :( ");
    Serial.println(" -> Restarting...");
    delay(15000);
    ESP.restart();
  }

  if (!client.connected())
  {

    long now = millis();

    if (now - lastReconnectAttemp > 5000)
    {
      lastReconnectAttemp = millis();
      if (reconnect())
      {
        lastReconnectAttemp = 0;
      }
    }
  }
  else
  {
    client.loop();
    process_sensors();
    send_data_to_broker();
  }
}

bool get_mqtt_credentials()
{

  Serial.print("\n\n\nGetting MQTT Credentials from WebHook");
  delay(1000);

  String toSend = "dId=" + dId + "&password=" + webhook_pass;

  HTTPClient http;
  http.begin(webhook_endpoint);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int response_code = http.POST(toSend);

  if (response_code < 0)
  {
    Serial.print("\n\n         Error Sending Post Request :( ");
    http.end();
    return false;
  }

  if (response_code != 200)
  {
    Serial.print("\n\n         Error in response :(   e-> " + response_code);
    http.end();
    return false;
  }

  if (response_code == 200)
  {
    String responseBody = http.getString();

    Serial.print("\n\n         Mqtt Credentials Obtained Successfully :) ");

    deserializeJson(mqtt_data_doc, responseBody);
    http.end();
    delay(1000);
  }

  return true;
}

void clear()
{
  Serial.write(27);    // ESC command
  Serial.print("[2J"); // clear screen command
  Serial.write(27);
  Serial.print("[H"); // cursor to home command
}