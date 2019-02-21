#ifndef __AWSWEBSOCKETCLIENT_H_
#define __AWSWEBSOCKETCLIENT_H_

#include <Arduino.h>
#include <Stream.h>
#include "Client.h"
#include "WebSocketsClient.h"
#include "CircularByteBuffer.h"
#include "sha256.h"
#include "Utils.h"
#include <time.h>

static const int HASH_HEX_LEN2 = 64;


//#define DEBUG_WEBSOCKET_MQTT(...) os_printf( __VA_ARGS__ )

#ifndef DEBUG_WEBSOCKET_MQTT
#define DEBUG_WEBSOCKET_MQTT(...)
#define NODEBUG_WEBSOCKET_MQTT
#endif



class AWSWebSocketClient : public Client, private WebSocketsClient {
public:

  //bufferSize defines the size of the circular byte buffer that provides the interface between messages arrived in websocket layer and byte reads from mqtt layer	
  AWSWebSocketClient (unsigned int bufferSize = 1000, unsigned long connectionTimeout = 50000);
  ~AWSWebSocketClient();

  

  int connect(IPAddress ip, uint16_t port);
  int connect(const char *host, uint16_t port);

  void putMessage (byte* buffer, int length);
  size_t write(uint8_t b);
  size_t write(const uint8_t *buf, size_t size);
  int available();
  int read();
  int read(uint8_t *buf, size_t size);

  int peek();
  void flush();
  void stop();
  uint8_t connected() ;
  operator bool();

  bool getUseSSL ();
  
  AWSWebSocketClient& setUseSSL (bool value);
  AWSWebSocketClient& setAWSRegion(const char * awsRegion);
  AWSWebSocketClient& setAWSDomain(const char * awsDomain);  
  AWSWebSocketClient& setAWSSecretKey(const char * awsSecKey);
  AWSWebSocketClient& setAWSKeyID(const char * awsKeyID);  
  AWSWebSocketClient& setPath(const char * path);
  AWSWebSocketClient& setAWSToken(const char * awsToken);
  

      
  protected:
  //generate AWS signed path
  char* generateAWSPath (uint16_t port);
  
  //convert the month info
  String getMonth(String sM);
  //get current time (UTC) from aws service (used to sign)
  String getCurrentTimeAmazon(void);

  String getCurrentTimeNTP(void);

  String ntpFixNumber (int number);
  
  //static instance of aws websocket client
  static AWSWebSocketClient* instance;
  //keep the connection state
  bool _connected;  
  //websocket callback
  static void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
  
  private:
  
  //enable ssl... if your using mqtt over websockets at AWS IoT service, it must be enabled
  bool useSSL;

  //connection timeout 
  unsigned long _connectionTimeout;

  //useAmazonTimestamp
  bool _useAmazonTimestamp;

  char* path;
  /* Name of region, eg. "us-east-1" in "kinesis.us-east-1.amazonaws.com". */
  char* awsRegion;
  /* Domain, optional, eg. "A2MBBEONHC9LUG.iot.us-east-1.amazonaws.com". */
  char* awsDomain;
  /* The user's AWS Secret Key for accessing the AWS Resource. */
  char* awsSecKey;
  /* The user's AWS Access Key ID for accessing the AWS Resource. */
  char* awsKeyID;
  /* The user's AWS Security Token for temporary credentials (just use with AWS STS). */
  char* awsToken;
  
  unsigned long lastTimeUpdate;
  //circular buffer to keep incoming messages from websocket
  CircularByteBuffer bb;
  
  //connection to get current time
  WiFiClient timeClient;
};

#endif
