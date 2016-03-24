#include <Arduino.h>
#include <Stream.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

//AWS
#include "sha256.h"
#include "Utils.h"
#include "AWSClient4.h"

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
    WiFiMulti.addAP("ssid", "wifipass");    
    Serial.println ("connecting to wifi");
    while(WiFiMulti.run() != WL_CONNECTED) {
        delay(100);
        Serial.print (".");
    }   
    Serial.println ("");

    //fill AWS parameters
    char hostname[] = "endpoint.iot.us-west-2.amazonaws.com";
    int port = 443; 
    awsWSclient.setAWSRegion("us-west-2");
    awsWSclient.setAWSDomain(hostname);
    awsWSclient.setAWSKeyID("yourAWSKey");
    awsWSclient.setAWSSecretKey("yourAWSSecretKey");
    
    awsWSclient.setUseSSL(true);
    int rc = ipstack.connect(hostname, port);
    if (rc != 1)
    {
      Serial.print("error connection to the websocket server");      
      return;
    }   
    //FIX (workaround) wait for websocket connection message... it should be inside ipstack.connect 
    for (int i=0; i<200; i+=1) {
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
    const char* topic = "esp8266-sample";
    MQTT::Message message;
    char buf[100];
    strcpy(buf, "Hello World! QoS 0 message");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    rc = client.publish(topic, message);

    //subscript to a topic
    rc = client.subscribe(topic, MQTT::QOS1, messageArrived);   
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
