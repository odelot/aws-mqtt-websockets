#include "AWSWebSocketClient.h"

//work as a singleton to be used by websocket layer message callback
AWSWebSocketClient* AWSWebSocketClient::instance = NULL;

//callback to handle messages coming from the websocket layer
void AWSWebSocketClient::webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            DEBUG_WEBSOCKET_MQTT("[AWSc] Disconnected!\n");
			AWSWebSocketClient::instance->stop ();			
            break;
        case WStype_CONNECTED:            
            DEBUG_WEBSOCKET_MQTT("[AWSc] Connected to url: %s\n",  payload);
			AWSWebSocketClient::instance->_connected = true;
            break;
        case WStype_TEXT:
            DEBUG_WEBSOCKET_MQTT("[WSc] get text: %s\n", payload);
            AWSWebSocketClient::instance->putMessage (payload, length);
            break;
        case WStype_BIN:
            DEBUG_WEBSOCKET_MQTT("[WSc] get binary length: %u\n", length);
            //hexdump(payload, length);
            AWSWebSocketClient::instance->putMessage (payload, length);
            break;
    }
	
		

}

//constructor
AWSWebSocketClient::AWSWebSocketClient (unsigned int bufferSize) {
    useSSL = true;
    connectionTimeout = 5000; //5 seconds
    AWSWebSocketClient:instance = this;
    onEvent(AWSWebSocketClient::webSocketEvent);
    awsRegion = NULL;
    awsSecKey = NULL;
    awsKeyID = NULL;
    awsDomain = NULL;
    path = NULL;
	_connected = false;	
    bb.init (bufferSize); //1000 bytes of circular buffer... maybe it is too big
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
String AWSWebSocketClient::getMonth(String sM) {
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
char* AWSWebSocketClient::getCurrentTime(void) {

	
    int timeout_busy = 0;

    String GmtDate;
    char* dateStamp = new char[15]();
	strcpy (dateStamp,"19860618123600");	
    
    if (timeClient.connect("aws.amazon.com", 80)) {

      // send a bad header on purpose, so we get a 400 with a DATE: timestamp
      //Send Request
      timeClient.println("GET example.com/ HTTP/1.1");
      timeClient.println("Connection: close");
      timeClient.println();

      while((!timeClient.available()) && (timeout_busy++ < 5000)){
        // Wait until the client sends some data
        delay(1);
      }

      // kill client if timeout
      if(timeout_busy >= 5000) {
          timeClient.flush();
          timeClient.stop();		  
          DEBUG_WEBSOCKET_MQTT("timeout receiving timeserver data");
		  
          return dateStamp;
      }

      // read the http GET Response
      String req2 = timeClient.readString();

      // close connection
      delay(1);
      timeClient.flush();
      timeClient.stop();
	  

      int ipos = req2.indexOf("Date:");
      if(ipos > 0) {
        String gmtDate = req2.substring(ipos, ipos + 35);
        String utctime = gmtDate.substring(18,22) + getMonth(gmtDate.substring(14,17)) + gmtDate.substring(11,13) + gmtDate.substring(23,25) + gmtDate.substring(26,28) + gmtDate.substring(29,31);
        utctime.substring(0, 14).toCharArray(dateStamp, 15);
      }
    }
    else {
       DEBUG_WEBSOCKET_MQTT("did not connect to timeserver");
    }

   
    timeout_busy=0;     // reset timeout
	
	
    return dateStamp;   // Return latest or last good dateStamp
}

//generate AWS url path, signed using url parameters
char* AWSWebSocketClient::generateAWSPath (uint16_t port) {

	 
    char* dateTime = getCurrentTime ();
    char* awsService = "iotdevicegateway";
    char* awsDate = new char[9]();
    strncpy(awsDate, dateTime, 8);
    awsDate[8] = '\0';
    char* awsTime = new char[7]();
    strncpy(awsTime, dateTime + 8, 6);
    awsTime[6] = '\0';
    delete[] dateTime;
	char* credentialScope = new char[strlen(awsDate)+strlen(awsRegion)+strlen(awsService)+16]();
	sprintf(credentialScope, "%s/%s/%s/aws4_request",awsDate,awsRegion,awsService);
	String key_credential (awsKeyID);
	key_credential+="/";
	key_credential+=credentialScope;
	//FIX tried a lot of escape functions, but no one was equal to escapeURL from javascript
	key_credential.replace ("/","%2F");
	const char* method = "GET";
	const char* canonicalUri = "/mqtt";
	const char* algorithm = "AWS4-HMAC-SHA256";

	char* canonicalQuerystring = new char[strlen(algorithm)+strlen(key_credential.c_str())+strlen(awsDate)+strlen(awsTime)+200]();
	sprintf(canonicalQuerystring, "%sX-Amz-Algorithm=%s", canonicalQuerystring,algorithm);
	sprintf(canonicalQuerystring, "%s&X-Amz-Credential=%s", canonicalQuerystring,key_credential.c_str());
	sprintf(canonicalQuerystring, "%s&X-Amz-Date=%sT%sZ", canonicalQuerystring, awsDate,awsTime);
	sprintf(canonicalQuerystring, "%s&X-Amz-Expires=86400", canonicalQuerystring); //sign will last one day
	sprintf(canonicalQuerystring, "%s&X-Amz-SignedHeaders=host", canonicalQuerystring);

	String portString = String(port);
	char* canonicalHeaders = new char[strlen (awsDomain)+ strlen (portString.c_str())+ 8]();

	sprintf(canonicalHeaders, "%shost:%s:%s\n", canonicalHeaders,awsDomain,portString.c_str());
	SHA256* sha256 = new SHA256();
	char* payloadHash = (*sha256)("", 0);
	delete sha256;
	char* canonicalRequest = new char[strlen (method)+ strlen (canonicalUri)+strlen(canonicalQuerystring)+strlen(canonicalHeaders)+strlen(payloadHash)+10]();

	sprintf(canonicalRequest, "%s%s\n%s\n%s\n%s\nhost\n%s", canonicalRequest,method,canonicalUri,canonicalQuerystring,canonicalHeaders,payloadHash);
	delete[] payloadHash;
	delete[] canonicalHeaders;


	sha256 = new SHA256();
	char* requestHash = (*sha256)(canonicalRequest, strlen (canonicalRequest));
	delete sha256;
	char* stringToSign = new char[strlen (algorithm)+ strlen(awsDate)+strlen (awsTime)+strlen(credentialScope)+strlen(requestHash)+6]();
	sprintf(stringToSign, "%s%s\n%sT%sZ\n%s\n%s", stringToSign,algorithm,awsDate,awsTime,credentialScope,requestHash);
	delete[] requestHash;
	delete[] credentialScope;
	delete[] canonicalRequest;	
	delete[] awsTime;

	/* Allocate memory for the signature */
    char* signature = new char[HASH_HEX_LEN2 + 1]();

    /* Create the signature key */
    /* + 4 for "AWS4" */
    int keyLen = strlen(awsSecKey) + 4;
    char* key = new char[keyLen + 1]();
    sprintf(key, "AWS4%s", awsSecKey);

    char* k1 = hmacSha256(key, keyLen, awsDate, strlen(awsDate));
	delete[] awsDate;
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

	char* requestUri = new char[strlen(canonicalUri)+strlen(canonicalQuerystring)+2]();
	//sprintf(requestUri, "%swss://%s%s?%s", requestUri, awsDomain,canonicalUri,canonicalQuerystring);
	sprintf(requestUri, "%s%s?%s", requestUri, canonicalUri,canonicalQuerystring);
	delete[] canonicalQuerystring;

	return requestUri;

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
	//make sure it is disconnect first
	  const char* protocol = "mqtt";
	  stop ();	
	  char* path = this->path;
	  //just need to free path if it was generated to connect to AWS
	  bool freePath = false;
	  if (this->path == NULL) {
		  //just generate AWS Path if user does not inform its own (to support the lib usage out of aws)
		  path = generateAWSPath (port);
		  freePath = true;
	  }
	  if (useSSL == true)
		  beginSSL (host,port,path,"",protocol);
	  else
		  begin (host,port,path,protocol);
	  long now = millis ();
	  while ( (millis ()-now) < connectionTimeout) {
		  loop ();		  
		  if (connected () == 1) {		  
			  if (freePath == true)
				delete[] path;
			  return 1;
		  }
		  delay (10);
	  }
	  if (freePath == true)
		delete[] path;
	  return 0;
}

//store messages arrived by websocket layer to be consumed by mqtt layer through the read funcion
void AWSWebSocketClient::putMessage (byte* buffer, int length) {
	bb.push (buffer,length);	
}

size_t AWSWebSocketClient::write(uint8_t b) {
	if (_connected == false)
	  return -1;
  return write (&b,1);
}

//write through websocket layer
size_t AWSWebSocketClient::write(const uint8_t *buf, size_t size) {
  if (_connected == false)
	  return -1;
  if (sendBIN (buf,size))
	  return size;
  return 0;
}

//return with there is bytes to consume from the circular buffer (used by mqtt layer)
int AWSWebSocketClient::available(){
  //force websocket to handle it messages
  if (_connected == false)
	  return false;
  loop ();
  return bb.getSize ();
}

//read from circular buffer (used by mqtt layer)
int AWSWebSocketClient::read() {
	if (_connected == false)
	  return -1;
	return bb.pop ();
}

//read from circular buffer (used by mqtt layer)
int AWSWebSocketClient::read(uint8_t *buf, size_t size) {
	if (_connected == false)
	  return -1;
	int s = size;
	if (bb.getSize()<s)
		s = bb.getSize ();

	bb.pop (buf,s);	
	return s;
};

int AWSWebSocketClient::peek() {
	return bb.peek ();
}

void AWSWebSocketClient::flush() {

}

void AWSWebSocketClient::stop() {
	if (_connected == true) {		
		_connected = false;
		bb.clear ();
	}
	disconnect ();
}

uint8_t AWSWebSocketClient::connected() {
return _connected;
};

AWSWebSocketClient::operator bool() {
	return _connected;
};


bool AWSWebSocketClient::getUseSSL () {
  return useSSL;
}

AWSWebSocketClient& AWSWebSocketClient::setUseSSL (bool value) {
  useSSL = value;
  return *this;
}
