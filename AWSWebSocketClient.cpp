#include "AWSWebSocketClient.h"

//work as a singleton to be used by websocket layer message callback
AWSWebSocketClient* AWSWebSocketClient::instance = NULL;

String AWSWebSocketClient::ntpFixNumber (int number) {
    String ret = "";
    if (number < 10) {
        ret = "0";
        ret += number;
        return ret;
    }
    else{
        ret += number;
        return ret;
    } 
}

String AWSWebSocketClient::getCurrentTimeNTP (void) {
  time_t now = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  boolean refreshTime = false;
  if ( (millis () - lastTimeUpdate) > 86400000L) {
    refreshTime = true;
    lastTimeUpdate = millis ();
  }
  if (timeinfo.tm_year==70) {

    int _try = 0;
    int maxTries = 3;
    while (timeinfo.tm_year==70 && _try < maxTries || refreshTime == true) {
        //Serial.println ("refresh time");
        configTime(1, 0, "pool.ntp.org", "time.nist.gov");
        now = time(nullptr);
        
        while (now < 1 * 2) {
            delay(500);  
            now = time(nullptr);
        }       
        
        gmtime_r(&now, &timeinfo); 
        _try+=1;
        delay (500);
    }
  }
  //Serial.print("Current time: ");
  //Serial.print(asctime(&timeinfo));
  String date = "";
  date += (1900+timeinfo.tm_year);
  date += ntpFixNumber(timeinfo.tm_mon+1);
  date += ntpFixNumber(timeinfo.tm_mday);
  date += ntpFixNumber(timeinfo.tm_hour);
  date += ntpFixNumber(timeinfo.tm_min);
  date += ntpFixNumber(timeinfo.tm_sec);
  
  return date;
}




//callback to handle messages coming from the websocket layer
void AWSWebSocketClient::webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            DEBUG_WEBSOCKET_MQTT("[AWSc] Disconnected!\n");
			AWSWebSocketClient::instance->stop ();			
            break;
        case WStype_CONNECTED:            
            DEBUG_WEBSOCKET_MQTT("[AWSc] Connected to url: %d %s\n", length, payload);
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
AWSWebSocketClient::AWSWebSocketClient (unsigned int bufferSize, unsigned long connectionTimeout) {
    useSSL = true;
    _connectionTimeout = connectionTimeout;
    AWSWebSocketClient:instance = this;
    onEvent(AWSWebSocketClient::webSocketEvent);
    awsRegion = NULL;
    awsSecKey = NULL;
    awsKeyID = NULL;
    awsDomain = NULL;
	awsToken = NULL;
    path = NULL;
	_connected = false;	
    bb.init (bufferSize); 
    lastTimeUpdate = 0;
    _useAmazonTimestamp = false;
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
	if (awsToken != NULL)
        delete[] awsToken;
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
String AWSWebSocketClient::getCurrentTimeAmazon (void) {

	
    int timeout_busy = 0;

    String GmtDate;
    String dateStamp;
    dateStamp ="19860618123600";
    
    if (timeClient.connect(("aws.amazon.com"), 80)) {

      // send a bad header on purpose, so we get a 400 with a DATE: timestamp
      //Send Request
      timeClient.println(F("GET example.com/ HTTP/1.1"));
      timeClient.println(F("Connection: close"));
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
        dateStamp = utctime.substring(0, 14);
      }
    }
    else {
       DEBUG_WEBSOCKET_MQTT("did not connect to timeserver");
    }

   
    timeout_busy=0;     // reset timeout
	
	
    return dateStamp;   // Return latest or last good dateStamp
}


/* Converts an integer value to its hex character*/
char to_hex(char code) {
  static char hex[] = "0123456789ABCDEF";
  return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char* url_encode(const char* str) {
  const char* pstr = str;
  char *buf = (char*) malloc(strlen(str) * 3 + 1), *pbuf = buf;
  while (*pstr) {
    if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') 
      *pbuf++ = *pstr;
    else if (*pstr == ' ') 
      *pbuf++ = '+';
    else 
      *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
    pstr++;
  }
  *pbuf = '\0';
  return buf;
}


//generate AWS url path, signed using url parameters
char* AWSWebSocketClient::generateAWSPath (uint16_t port) {
    
    String dateTime;
    if (_useAmazonTimestamp) {
        dateTime = getCurrentTimeAmazon ();
    } else  {
        dateTime = getCurrentTimeNTP ();
    }
    DEBUG_WEBSOCKET_MQTT (dateTime.c_str());    
    String awsService = F("iotdevicegateway");
    char awsDate[9];
    strncpy(awsDate, dateTime.c_str(), 8);
    awsDate[8] = '\0';
    char awsTime[7];
    strncpy(awsTime, dateTime.c_str() + 8, 6);
    awsTime[6] = '\0';
    
	char credentialScope[strlen(awsDate)+strlen(awsRegion)+strlen(awsService.c_str())+16];
	sprintf(credentialScope, "%s/%s/%s/aws4_request",awsDate,awsRegion,awsService.c_str());
	String key_credential (awsKeyID);
	key_credential+="/";
	key_credential+=credentialScope;
	//FIX tried a lot of escape functions, but no one was equal to escapeURL from javascript
	char* ekey_credential = url_encode (key_credential.c_str ());
	key_credential = ekey_credential;
	free (ekey_credential);
	
	
	String method = F("GET");
	String canonicalUri = F("/mqtt");
	String algorithm = F("AWS4-HMAC-SHA256");
	String token = "";
	//adding AWS STS security token for temporary AIM credentials
	if (awsToken != NULL) {
		char* eToken = url_encode (awsToken);
		token = eToken;
		free (eToken);

	}
	
	char canonicalQuerystring [strlen(algorithm.c_str())+strlen(key_credential.c_str())+strlen(awsDate)+strlen(awsTime)+strlen (token.c_str())+225];
	sprintf(canonicalQuerystring, "X-Amz-Algorithm=%s", algorithm.c_str());
	sprintf(canonicalQuerystring, "%s&X-Amz-Credential=%s", canonicalQuerystring,key_credential.c_str());
	sprintf(canonicalQuerystring, "%s&X-Amz-Date=%sT%sZ", canonicalQuerystring, awsDate,awsTime);
	sprintf(canonicalQuerystring, "%s&X-Amz-Expires=86400", canonicalQuerystring); //sign will last one day
	sprintf(canonicalQuerystring, "%s&X-Amz-SignedHeaders=host", canonicalQuerystring);

	String portString = String(port);
	char canonicalHeaders [strlen (awsDomain)+ strlen (portString.c_str())+ 8];

	sprintf(canonicalHeaders, "host:%s:%s\n", awsDomain,portString.c_str());
	SHA256* sha256 = new SHA256();
	char* payloadHash = (*sha256)("", 0);
	delete sha256;
	char canonicalRequest [strlen (method.c_str())+ strlen (canonicalUri.c_str())+strlen(canonicalQuerystring)+strlen(canonicalHeaders)+strlen(payloadHash)+10];

	sprintf(canonicalRequest, "%s\n%s\n%s\n%s\nhost\n%s", method.c_str(),canonicalUri.c_str(),canonicalQuerystring,canonicalHeaders,payloadHash);
	delete[] payloadHash;
	


	sha256 = new SHA256();
	char* requestHash = (*sha256)(canonicalRequest, strlen (canonicalRequest));
	delete sha256;
	char stringToSign[strlen (algorithm.c_str())+ strlen(awsDate)+strlen (awsTime)+strlen(credentialScope)+strlen(requestHash)+6];
	sprintf(stringToSign, "%s\n%sT%sZ\n%s\n%s", algorithm.c_str(),awsDate,awsTime,credentialScope,requestHash);
	delete[] requestHash;

	/* Allocate memory for the signature */
    char signature [HASH_HEX_LEN2 + 1];

    /* Create the signature key */
    /* + 4 for "AWS4" */
    int keyLen = strlen(awsSecKey) + 4;
    char key[keyLen + 1];
    sprintf(key, "AWS4%s", awsSecKey);

    char* k1 = hmacSha256(key, keyLen, awsDate, strlen(awsDate));	
    
    char* k2 = hmacSha256(k1, SHA256_DEC_HASH_LEN, awsRegion,
            strlen(awsRegion));

    delete[] k1;
    char* k3 = hmacSha256(k2, SHA256_DEC_HASH_LEN, awsService.c_str(),
            strlen(awsService.c_str()));

	delete[] k2;
    char* k4 = hmacSha256(k3, SHA256_DEC_HASH_LEN, "aws4_request", 12);

	delete[] k3;
    char* k5 = hmacSha256(k4, SHA256_DEC_HASH_LEN, stringToSign, strlen(stringToSign));

	delete[] k4;	
    // Convert the chars in hash to hex for signature.
    for (int i = 0; i < SHA256_DEC_HASH_LEN; ++i) {
        sprintf(signature + 2 * i, "%02lx", 0xff & (unsigned long) k5[i]);
    }
    delete[] k5;

	sprintf(canonicalQuerystring, "%s&X-Amz-Signature=%s", canonicalQuerystring, signature);
	
	//adding AWS STS security token for temporary AIM credentials
	if (awsToken != NULL) {		
		sprintf(canonicalQuerystring, "%s&X-Amz-Security-Token=%s", canonicalQuerystring,token.c_str());		
	}
	
	char* requestUri = new char[strlen(canonicalUri.c_str())+strlen(canonicalQuerystring)+2]();	
	sprintf(requestUri, "%s?%s", canonicalUri.c_str(),canonicalQuerystring);
	
	return requestUri;
	 

}

AWSWebSocketClient& AWSWebSocketClient::setAWSRegion(const char * awsRegion) {
	if (this->awsRegion != NULL)
        delete[] this->awsRegion;
    int len = strlen(awsRegion) + 1;
    this->awsRegion = new char[len]();
    strcpy(this->awsRegion, awsRegion);
	return *this;
}

AWSWebSocketClient& AWSWebSocketClient::setAWSDomain(const char * awsDomain) {
	if (this->awsDomain != NULL)
        delete[] this->awsDomain;
    int len = strlen(awsDomain) + 1;
    this->awsDomain = new char[len]();
    strcpy(this->awsDomain, awsDomain);
	return *this;
}

AWSWebSocketClient& AWSWebSocketClient::setAWSSecretKey(const char * awsSecKey) {
    if (this->awsSecKey != NULL) 
	{
		if (strlen (awsSecKey) ==  strlen(this->awsSecKey)){
			strcpy(this->awsSecKey, awsSecKey);
			return *this;
		} else {			
			delete[] this->awsSecKey;	
		}
        
	}
	int len = strlen(awsSecKey) + 1;
    this->awsSecKey = new char[len]();
    strcpy(this->awsSecKey, awsSecKey);
	return *this;
}
AWSWebSocketClient& AWSWebSocketClient::setAWSKeyID(const char * awsKeyID) {
    if (this->awsKeyID != NULL) {
		if (strlen (awsKeyID) ==  strlen(this->awsKeyID)){
			strcpy(this->awsKeyID, awsKeyID);
			return *this;
		} else {			
			delete[] this->awsKeyID;	
		}	
	}
	
	int len = strlen(awsKeyID) + 1;
    this->awsKeyID = new char[len]();
    strcpy(this->awsKeyID, awsKeyID);
	return *this;
}

AWSWebSocketClient& AWSWebSocketClient::setPath(const char * path) {
    if (this->path != NULL)
        delete[] this->path;
	int len = strlen(path) + 1;
    this->path = new char[len]();
    strcpy(this->path, path);
	return *this;
}

AWSWebSocketClient& AWSWebSocketClient::setAWSToken(const char * awsToken) {
	if (this->awsToken != NULL)
       {
		if (strlen (awsToken) ==  strlen(this->awsToken)){
			strcpy(this->awsToken, awsToken);
			return *this;
		} else {			
			delete[] this->awsToken;	
		}	
	}
	int len = strlen(awsToken) + 1;
    this->awsToken = new char[len]();
    strcpy(this->awsToken, awsToken);
	return *this;
}

int AWSWebSocketClient::connect(IPAddress ip, uint16_t port){
	  return connect (awsDomain,port);
}

int AWSWebSocketClient::connect(const char *host, uint16_t port) {
	  //disconnect first
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
		  beginSSL (host,port,path,"",F("mqtt"));
	  else
		  begin (host,port,path,F("mqtt"));
	  long now = millis ();
	  while ( (millis ()-now) < _connectionTimeout) {
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
	}
    bb.clear ();
	clientDisconnect(&_client);
	//disconnect ();
}

uint8_t AWSWebSocketClient::connected() {
    DEBUG_WEBSOCKET_MQTT("connected");
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
