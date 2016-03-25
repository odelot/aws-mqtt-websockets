#include <Arduino.h>
#include <Stream.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

//AWS
#include "sha256.h"
#include "Utils.h"
#include "AWSClient2.h"

//WEBSockets
#include <Hash.h>
#include <WebSocketsClient.h>

//MQTT PAHO
#include <SPI.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>


//AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "ByteBuffer.h"

//AWS IOT config, change these:
char wifi_ssid[]       = "your-ssid";
char wifi_password[]   = "your-password";
char aws_endpoint[]    = "your-endpoint.iot.eu-west-1.amazonaws.com";
char aws_key[]         = "your-iam-key";
char aws_secret[]      = "your-iam-secret-key";
char aws_region[]      = "eu-west-1";
const char* aws_topic  = "$aws/things/your-device/shadow/update";

ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient;

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, 50, 1> client = MQTT::Client<IPStack, Countdown, 50, 1>(ipstack);


//generate random mqtt clientID
char* generateClientID () {
  char* cID = new char[23]();
  for (int i=0; i<22; i+=1)
    cID[i]=(char)random(1, 256);
  return cID;
}

//count messages arrived
int arrivedcount = 0;

//callback to handle mqtt messages
void messageArrived(MQTT::MessageData& md)
{
  MQTT::Message &message = md.message;

  Serial.print("Message ");
  Serial.print(++arrivedcount);
  Serial.print(" arrived: qos ");
  Serial.print(message.qos);
  Serial.print(", retained ");
  Serial.print(message.retained);
  Serial.print(", dup ");
  Serial.print(message.dup);
  Serial.print(", packetid ");
  Serial.println(message.id);
  Serial.print("Payload ");
  char* msg = new char[message.payloadlen+1]();
  memcpy (msg,message.payload,message.payloadlen);
  Serial.println(msg);
  delete msg;
}

void setup() {
    Serial.begin (115200);
    delay (2000);
    //Serial.setDebugOutput(1);

    //fill with ssid and wifi password
    WiFiMulti.addAP(wifi_ssid, wifi_password);
    Serial.println ("connecting to wifi");
    while(WiFiMulti.run() != WL_CONNECTED) {
        delay(100);
        Serial.print (".");
    }
    Serial.println ("Connected");

    //fill AWS parameters
    int port = 443;
    awsWSclient.setAWSRegion(aws_region);
    awsWSclient.setAWSDomain(aws_endpoint);
    awsWSclient.setAWSKeyID(aws_key);
    awsWSclient.setAWSSecretKey(aws_secret);

    awsWSclient.setUseSSL(true);
    int rc = ipstack.connect(aws_endpoint, port);
    if (rc != 1)
    {
      Serial.println("error connection to the websocket server");
      return;
    } else {
      Serial.println("socket connected");
    }
    //FIX (workaround) wait for websocket connection message... it should be inside ipstack.connect
    for (int i=0; i<200; i+=1) {
      Serial.print(".");
       awsWSclient.available ();
       delay (10);
    }

    Serial.println("MQTT connecting");
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = generateClientID ();
    rc = client.connect(data);
    if (rc != 0)
    {
      Serial.print("error connection to MQTT server");
      Serial.println(rc);
    }
    Serial.println("MQTT connected");

    //send a message
    MQTT::Message message;
    char buf[100];
    strcpy(buf, "{\"reported\":{\"foo\": \"bar\"}, desired:{}}");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    rc = client.publish(aws_topic, message);

    //subscript to a topic
    rc = client.subscribe(aws_topic, MQTT::QOS1, messageArrived);
    if (rc != 0)
    {
      Serial.print("rc from MQTT subscribe is ");
      Serial.println(rc);
    }
    Serial.println("MQTT subscribed");

}

void loop() {
  //keep the mqtt up and running
  client.yield();
}
