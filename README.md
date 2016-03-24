# aws-mqtt-websockets
Implementation of a middeware to use AWS MQTT service through websockets. aiming esp8266 plataform

## ChangeLog
  
* 0.1 - has known limitation and it was not extensively tested

## Motivation

As we cannot use AWS MQTT service directlybecause of the lack of support for TLS 1.2, we need to use the websocket communication as a transport layer for MQTT through SSL (supported by esp8266)

This way we can change the state of your esp8266 devices in realtime, without using the AWS Restful API and busy-waiting inefficient approach.

## Dependencies

| Library                   | Link                                                            | Use                 |
|---------------------------|-----------------------------------------------------------------|---------------------|
|aws-sdk-arduino            |https://github.com/svdgraaf/aws-sdk-arduino                      |aws signing functions|
|arduinoWebSockets          |https://github.com/Links2004/arduinoWebSockets                   |websocket comm impl  |
|Paho MQTT for Arduino      |https://projects.eclipse.org/projects/technology.paho/downloads  |mqtt comm impl       |
|ByteBuffer\*                |https://github.com/siggiorn/arduino-buffered-serial              |circular byte buffer |

\* You may need to delete BufferedSerial as it is not necessary and not compatible with esp8266 (I will probably implement my own version of this circular byte array)


## Limitation

* does not implemented reconnect functionality yet (it drops the connection after a couple minutes in idle)
* it was not extensively tested (my only test was in the example file, publish and subscribe to QoS0 and QoS1 with success)
* may work in arduino without AWS and SSL (but not tested yet, you need to turn off SSL and inform the path - there is no point to use it with arduino, as you does not have SSL. it is better to use MQTT directy without a security layer)
* a lot of TODO and FIX in the code - i think it is more important to share first ;-)

## Usage 

It is transparent. It is the same as the usage of Paho. There is just some changes in the connection step. See the example for details.
