#include "AWSWebSocketClient.h"

//work as a singleton to be used by websocket layer message callback
AWSWebSocketClient* AWSWebSocketClient::instance = NULL;

//callback to handle messages coming from the websocket layer
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            DEBUG_WEBSOCKET_MQTT("[AWSc] Disconnected!\n");
            break;
        case WStype_CONNECTED:
            //TODO maybe control connection here
            DEBUG_WEBSOCKET_MQTT("[AWSc] Connected to url: %s\n",  payload);
            break;
        case WStype_TEXT:
            DEBUG_WEBSOCKET_MQTT("[WSc] get text: %s\n", payload);
            AWSWebSocketClient::instance->putMessage (payload, length);
            break;
        case WStype_BIN:
            DEBUG_WEBSOCKET_MQTT("[WSc] get binary length: %u\n", length);
            //hexdump(payload, lenght);
            AWSWebSocketClient::instance->putMessage (payload, length);
            break;
    }

}

//constructor
AWSWebSocketClient::AWSWebSocketClient () {
    useSSL = true;
    connectionTimeout = 5000; //5 seconds
    AWSWebSocketClient:instance = this;
    onEvent(webSocketEvent);
    awsRegion = NULL;
    awsSecKey = NULL;
    awsKeyID = NULL;
    awsDomain = NULL;
    path = NULL;
    bb.init (1000); //1000 bytes of circular buffer... maybe it is too big
}

//destructor
AWSWebSocketClient::~AWSWebSocketClient(void) {
    if (awsRegion != NULL)
        delete[] awsRegion;
    if (awsDomain != NULL)
        delete[] awsDomain;
    if (awsSecKey != NULL)
        delete[] awsSecKey;
    if (awsKeyID != NULL)
        delete[] awsKeyID;
    if (path != NULL)
        delete[] path;
}


// convert month to digits
String getMonth2(String sM) {
  if(sM=="Jan") return "01";
  if(sM=="Feb") return "02";
  if(sM=="Mar") return "03";
  if(sM=="Apr") return "04";
  if(sM=="May") return "05";
  if(sM=="Jun") return "06";
  if(sM=="Jul") return "07";
  if(sM=="Aug") return "08";
  if(sM=="Sep") return "09";
  if(sM=="Oct") return "10";
  if(sM=="Nov") return "11";
  if(sM=="Dec") return "12";
  return "01";
}

//get current time (UTC) from aws service (used to sign)
char* getCurrentTime2(void) {

    int timeout_busy = 0;

    String GmtDate;
    char* dateStamp = new char[15]();

    WiFiClient* client = new WiFiClient();;
    if (client->connect("aws.amazon.com", 80)) {

      // send a bad header on purpose, so we get a 400 with a DATE: timestamp
      char* timeServerGet = (char*)"GET example.com/ HTTP/1.1";

      //Send Request
      client->println("GET example.com/ HTTP/1.1");
      client->println("Connection: close");
      client->println();

      while((!client->available()) && (timeout_busy++ < 5000)){
        // Wait until the client sends some data
        delay(1);
      }

      // kill client if timeout
      if(timeout_busy >= 5000) {
          client->flush();
          client->stop();
          Serial.println("timeout receiving timeserver data");
          return dateStamp;
      }

      // read the http GET Response
      String req2 = client->readString();

      // close connection
      delay(1);
      client->flush();
      client->stop();

      int ipos = req2.indexOf("Date:");
      if(ipos > 0) {
        String gmtDate = req2.substring(ipos, ipos + 35);
        String utctime = gmtDate.substring(18,22) + getMonth2(gmtDate.substring(14,17)) + gmtDate.substring(11,13) + gmtDate.substring(23,25) + gmtDate.substring(26,28) + gmtDate.substring(29,31);
        utctime.substring(0, 14).toCharArray(dateStamp, 15);
      }
    }
    else {
      Serial.println("did not connect to timeserver");
    }

    delete client;
    timeout_busy=0;     // reset timeout

    return dateStamp;   // Return latest or last good dateStamp
}

//generate AWS url path, signed using url parameters
char* AWSWebSocketClient::generateAWSPath () {

	// set date and time
    const char* dateTime = getCurrentTime2 ();
    char* awsService = "iotdevicegateway";
    char* awsDate = new char[9]();
    strncpy(awsDate, dateTime, 8);
    awsDate[8] = '\0';
    char* awsTime = new char[7];
    strncpy(awsTime, dateTime + 8, 6);
    awsTime[6] = '\0';
    delete[] dateTime;
	char* credentialScope = new char[50]();
	sprintf(credentialScope, "%s/%s/%s/aws4_request",awsDate,awsRegion,awsService);
	String key_credential (awsKeyID);
	key_credential+="/";
	key_credential+=credentialScope;
	//FIX tried a lot of escape functions, but no one was equal to escapeURL from javascript
	key_credential.replace ("/","%2F");
	char* method = "GET";
	char* canonicalUri = "/mqtt";
	char* algorithm = "AWS4-HMAC-SHA256";

	char* canonicalQuerystring = new char[500]();
	sprintf(canonicalQuerystring, "%sX-Amz-Algorithm=%s", canonicalQuerystring,algorithm);
	sprintf(canonicalQuerystring, "%s&X-Amz-Credential=%s", canonicalQuerystring,key_credential.c_str());
	sprintf(canonicalQuerystring, "%s&X-Amz-Date=%sT%sZ", canonicalQuerystring, awsDate,awsTime);
	sprintf(canonicalQuerystring, "%s&X-Amz-Expires=86400", canonicalQuerystring, awsDomain); //sign will last one day
	sprintf(canonicalQuerystring, "%s&X-Amz-SignedHeaders=host", canonicalQuerystring, awsDomain);

	char* canonicalHeaders = new char[150]();
	sprintf(canonicalHeaders, "%shost:%s\n", canonicalHeaders,awsDomain);
	SHA256* sha256 = new SHA256();
	char* payloadHash = (*sha256)("", 0);
	delete sha256;
	char* canonicalRequest = new char[500]();

	sprintf(canonicalRequest, "%s%s\n%s\n%s\n%s\nhost\n%s", canonicalRequest,method,canonicalUri,canonicalQuerystring,canonicalHeaders,payloadHash);
	delete[] payloadHash;


	sha256 = new SHA256();
	char* requestHash = (*sha256)(canonicalRequest, strlen (canonicalRequest));
	delete sha256;
	char* stringToSign = new char[500]();
	sprintf(stringToSign, "%s%s\n%sT%sZ\n%s\n%s", stringToSign,algorithm,awsDate,awsTime,credentialScope,requestHash);
	delete[] requestHash;

	delete[] canonicalRequest;

	/* Allocate memory for the signature */
    char* signature = new char[HASH_HEX_LEN2 + 1]();

    /* Create the signature key */
    /* + 4 for "AWS4" */
    int keyLen = strlen(awsSecKey) + 4;
    char* key = new char[keyLen + 1]();
    sprintf(key, "AWS4%s", awsSecKey);

    char* k1 = hmacSha256(key, keyLen, awsDate, strlen(awsDate));

    delete[] key;
    char* k2 = hmacSha256(k1, SHA256_DEC_HASH_LEN, awsRegion,
            strlen(awsRegion));

    delete[] k1;
    char* k3 = hmacSha256(k2, SHA256_DEC_HASH_LEN, awsService,
            strlen(awsService));

	delete[] k2;
    char* k4 = hmacSha256(k3, SHA256_DEC_HASH_LEN, "aws4_request", 12);

	delete[] k3;
    char* k5 = hmacSha256(k4, SHA256_DEC_HASH_LEN, stringToSign, strlen(stringToSign));

	delete[] k4;
	delete[] stringToSign;
    /* Convert the chars in hash to hex for signature. */
    for (int i = 0; i < SHA256_DEC_HASH_LEN; ++i) {
        sprintf(signature + 2 * i, "%02lx", 0xff & (unsigned long) k5[i]);
    }
    delete[] k5;

	sprintf(canonicalQuerystring, "%s&X-Amz-Signature=%s", canonicalQuerystring, signature);
	delete[] signature;

	char* requestUri = new char[500]();
	//sprintf(requestUri, "%swss://%s%s?%s", requestUri, awsDomain,canonicalUri,canonicalQuerystring);
	sprintf(requestUri, "%s%s?%s", requestUri, canonicalUri,canonicalQuerystring);
	delete[] canonicalQuerystring;

	char* result = new char[strlen (requestUri)+1]();
	strcpy (result,requestUri);
	delete[] requestUri;

	return result;

}

AWSWebSocketClient& AWSWebSocketClient::setAWSRegion(const char * awsRegion) {
    int len = strlen(awsRegion) + 1;
    this->awsRegion = new char[len]();
    strcpy(this->awsRegion, awsRegion);
	return *this;
}

AWSWebSocketClient& AWSWebSocketClient::setAWSDomain(const char * awsDomain) {
    int len = strlen(awsDomain) + 1;
    this->awsDomain = new char[len]();
    strcpy(this->awsDomain, awsDomain);
	return *this;
}

AWSWebSocketClient& AWSWebSocketClient::setAWSSecretKey(const char * awsSecKey) {
    int len = strlen(awsSecKey) + 1;
    this->awsSecKey = new char[len]();
    strcpy(this->awsSecKey, awsSecKey);
	return *this;
}
AWSWebSocketClient& AWSWebSocketClient::setAWSKeyID(const char * awsKeyID) {
    int len = strlen(awsKeyID) + 1;
    this->awsKeyID = new char[len]();
    strcpy(this->awsKeyID, awsKeyID);
	return *this;
}

AWSWebSocketClient& AWSWebSocketClient::setPath(const char * path) {
    int len = strlen(path) + 1;
    this->path = new char[len]();
    strcpy(this->path, path);
	return *this;
}

int AWSWebSocketClient::connect(IPAddress ip, uint16_t port){
	  return connect (awsDomain,port);
}

int AWSWebSocketClient::connect(const char *host, uint16_t port) {
	char* path = this->path;
	  if (this->path == NULL)
		  path = generateAWSPath ();
	  if (useSSL == true)
		  beginSSL (host,port,path,"","mqtt");
	  else
		  begin (host,port,path,"mqtt");
	  long now = millis ();
	  while ( (millis ()-now) < connectionTimeout) {
		  loop ();
		  //this is not good, would be good that it just continue after conn message has been received
		  if (connected () == 1)
			  return 1;
		  delay (10);
	  }
	  return 0;
}

//store messages arrived by websocket layer to be consumed by mqtt layer through the read funcion
void AWSWebSocketClient::putMessage (byte* buffer, int length) {
	//TODO: make my own circular buffer to improve here
	for (int i=0; i< length; i+=1) {
		bb.put (buffer[i]);
	}
}

size_t AWSWebSocketClient::write(uint8_t b) {
  return write (&b,1);
}

//write through websocket layer
size_t AWSWebSocketClient::write(const uint8_t *buf, size_t size) {
  if (sendBIN (buf,size))
	  return size;
  return 0;
}

//return with there is bytes to consume from the circular buffer (used by mqtt layer)
int AWSWebSocketClient::available(){
  //force websocket to handle it messages
  loop ();
  return bb.getSize ();
}

//read from circular buffer (used by mqtt layer)
int AWSWebSocketClient::read() {
	return bb.get ();
}

//read from circular buffer (used by mqtt layer)
int AWSWebSocketClient::read(uint8_t *buf, size_t size) {
	int s = size;
	if (bb.getSize()<s)
		s = bb.getSize ();

	//TODO improve here when implement my own bytebuffer
	for (int i=0; i< s; i+=1) {
		buf[i]=bb.get ();

	}
	return s;
};

int AWSWebSocketClient::peek() {
	//TODO fix this with our own bytebuffer impl
	//bb.peek ();
	Serial.println ("PEEK <o>");
	return 0;
}

void AWSWebSocketClient::flush() {

}

void AWSWebSocketClient::stop() {
	disconnect ();
}

uint8_t AWSWebSocketClient::connected() {
return clientIsConnected(&_client);
};

AWSWebSocketClient::operator bool() {
	return clientIsConnected(&_client);
};


bool AWSWebSocketClient::getUseSSL () {
  return useSSL;
}

AWSWebSocketClient& AWSWebSocketClient::setUseSSL (bool value) {
  useSSL = value;
  return *this;
}
