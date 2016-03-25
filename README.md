# aws-mqtt-websockets
Implementation of a middleware to use AWS MQTT service through websockets. Aiming the esp8266 platform

## ChangeLog

* 0.1 - has known limitation and it was not extensively tested

## Motivation

As we cannot use AWS MQTT service directly because of the lack of support for TLS 1.2, we need to use the websocket communication as a transport layer for MQTT through SSL (supported by esp8266)

This way we can change the state of your esp8266 devices in realtime, without using the AWS Restful API and busy-waiting inefficient approach.

## Dependencies

| Library                   | Link                                                            | Use                 |
|---------------------------|-----------------------------------------------------------------|---------------------|
|aws-sdk-arduino            |https://github.com/svdgraaf/aws-sdk-arduino                      |aws signing functions|
|arduinoWebSockets          |https://github.com/Links2004/arduinoWebSockets                   |websocket comm impl  |
|Paho MQTT for Arduino      |https://projects.eclipse.org/projects/technology.paho/downloads  |mqtt comm impl       |
|ByteBuffer\*               |https://github.com/siggiorn/arduino-buffered-serial              |circular byte buffer |

\* You may need to delete BufferedSerial as it is not necessary and not compatible with esp8266 (I will probably implement my own version of this circular byte array)


## Limitation

* Does not implemented reconnect functionality yet (it drops the connection after a couple minutes in idle)
* It was not extensively tested (like stress test, with many messages. it was just simple tests, like subscribe and publich Qos0 and QoS1 messages)
* May work in arduino without SSL and out of aws environment (but not tested yet, you need to turn off SSL and inform the path - you may find no use for it with arduino, as arduino does not have SSL support. It is better to use MQTT directy without a security layer)
* A lot of TODO and FIX in the code - I think it is more important to share first ;-)

## Usage

It is transparent. It is the same as the usage of Paho. There is just some changes in the connection step. See the example for details. Things you should edit in the example:
* ssid and password to connect to wifi
* domain/endpoint for your aws iot service
* region of your aws iot service
* aws user key \*\*
* aws user secret key

 \*\* It is a good practice creating a new user (and grant just iot services permission). Avoid use the key/secret key of your main aws console user
