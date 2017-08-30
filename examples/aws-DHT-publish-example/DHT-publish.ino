#include <Arduino.h>
#include <Stream.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

// DHT22 - https://learn.adafruit.com/dht, https://github.com/adafruit/DHT-sensor-library
#include "DHT.h"

//AWS
#include "sha256.h"
#include "Utils.h"

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
#include "CircularByteBuffer.h"

//  --------- Config ---------- //
//AWS IOT config, change these:
char wifi_ssid[]       = "your-ssid";
char wifi_password[]   = "your-password";
char aws_endpoint[]    = "your-endpoint.iot.eu-west-1.amazonaws.com";
char aws_key[]         = "your-iam-key";
char aws_secret[]      = "your-iam-secret-key";
char aws_region[]      = "eu-west-1";
const char* aws_topic  = "$aws/things/your-device/shadow/update";
int port = 443;

// If stuff isn't working right, watch the console:
#define DEBUG_PRINT 1

#define DHTPIN 5
#define DHTTYPE DHT22 //Set for 11, 21, or 22

//MQTT config
const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 1;
// ---------- /Config ----------//

DHT dht(DHTPIN, DHTTYPE);
ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers> *client = NULL;

//# of connections
long connection = 0;

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

  if (DEBUG_PRINT) {
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
}

//connects to websocket layer and mqtt layer
bool connect () {

    if (client == NULL) {
      client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
    } else {

      if (client->isConnected ()) {    
        client->disconnect ();
      }  
      delete client;
      client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
    }


    //delay is not necessary... it just help us to get a "trustful" heap space value
    delay (1000);
    if (DEBUG_PRINT) {
      Serial.print (millis ());
      Serial.print (" - conn: ");
      Serial.print (++connection);
      Serial.print (" - (");
      Serial.print (ESP.getFreeHeap ());
      Serial.println (")");
    }

   int rc = ipstack.connect(aws_endpoint, port);
    if (rc != 1)
    {
      if (DEBUG_PRINT) {
        Serial.println("error connection to the websocket server");
      }
      return false;
    } else {
      if (DEBUG_PRINT) {
        Serial.println("websocket layer connected");
      }
    }
    
    if (DEBUG_PRINT) {
      Serial.println("MQTT connecting");
    }
    
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    char* clientID = generateClientID ();
    data.clientID.cstring = clientID;
    rc = client->connect(data);
    delete[] clientID;
    if (rc != 0)
    {
      if (DEBUG_PRINT) {
        Serial.print("error connection to MQTT server");
        Serial.println(rc);
        return false;
      }
    }
    if (DEBUG_PRINT) {
      Serial.println("MQTT connected");
    }
    return true;
}

//subscribe to a mqtt topic
void subscribe () {
   //subscrip to a topic
    int rc = client->subscribe(aws_topic, MQTT::QOS0, messageArrived);
    if (rc != 0) {
      if (DEBUG_PRINT) {
        Serial.print("rc from MQTT subscribe is ");
        Serial.println(rc);
      }
      return;
    }
    if (DEBUG_PRINT) {
      Serial.println("MQTT subscribed");
    }
}

void setup() {
    Serial.begin (115200);   
    WiFiMulti.addAP(wifi_ssid, wifi_password);
    
    while(WiFiMulti.run() != WL_CONNECTED) {
        delay(100);
        if (DEBUG_PRINT) {
          Serial.print (". ");
        }
    }
    if (DEBUG_PRINT) {
      Serial.println ("\nconnected to network " + String(wifi_ssid) + "\n");
    }

    //fill AWS parameters    
    awsWSclient.setAWSRegion(aws_region);
    awsWSclient.setAWSDomain(aws_endpoint);
    awsWSclient.setAWSKeyID(aws_key);
    awsWSclient.setAWSSecretKey(aws_secret);
    awsWSclient.setUseSSL(true);

    dht.begin();
}

void loop() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  delay(10000);
  String h = String(dht.readHumidity());    // Read temperature as Fahrenheit (isFahrenheit = true)
  String f = String(dht.readTemperature(true));
  
  if (isnan(dht.readHumidity()) || isnan(dht.readTemperature(true))) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  } else {
    if (DEBUG_PRINT) {
      Serial.print("Humidity: ");
      Serial.print(h);
      Serial.print(" %\t");
      Serial.print("Temperature: ");
      Serial.print(f);
      Serial.print(" *F\t\n");
    }
  };

  String values = "{\"state\":{\"reported\":{\"temp\": " + f + ",\"humidity\": " + h + "}}}";
  // http://stackoverflow.com/questions/31614364/arduino-joining-string-and-char
  const char *publish_message = values.c_str();

  //keep the mqtt up and running
  if (awsWSclient.connected ()) {    
      client->yield();
      
      subscribe (); 
      //publish 
      MQTT::Message message;
      char buf[1000];
      strcpy(buf, publish_message);
      message.qos = MQTT::QOS0;
      message.retained = false;
      message.dup = false;
      message.payload = (void*)buf;
      message.payloadlen = strlen(buf)+1;
      int rc = client->publish(aws_topic, message);  
  } else {
    //handle reconnection
    connect ();
  }
}
