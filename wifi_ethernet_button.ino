/*

 ***********************
 * Arduino WiFi button *
 ***********************

 -----------------------------------------------------------------------

  Copyright 2017-2021 by blauzaaan, https://github.com/blauzaaan

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

 -----------------------------------------------------------------------

  See Readme.md for detailed instructions.

  Adjust to your needs, particularly:
    * SERVER_IP
    * SERVER_PORT
    * BLUE_PIN
    * RED_PIN
    * BUTTON_PIN
    * TOKEN, MESSAGE_ON and MESSAGE_OFF (they should all use the same secret, unless you know what you are doing)
*/

//use Ethernet.h for official Arduino Ethernet shield or any WIZNet W5x00 adapters
// see https://www.arduino.cc/en/Reference/Ethernet
#include <Ethernet.h>

//use UIPEthernet.h for ENC28J60 based Ethernet adapters
//#include <UIPEthernet.h>

#include <Button.h>
#include <RGBLed.h>

// --------------------------------------------------------------------------------------------------
// CONFIGURATION
// --------------------------------------------------------------------------------------------------
// adjust these to your needs

#define SERVER_IP IPAddress(192, 168, 12, 1)
#define SERVER_PORT 5015

//randomly generated MAC address
const uint8_t mac[6] = { 0x80, 0x41, 0x9C, 0xDA, 0x64, 0xCE };


// don't use onboard LED as pin 13 is used by ethernet adapter!
// button RGB LED pins
#define GREEN_PIN 5
#define RED_PIN 6      //error indicator LED pin
#define BLUE_PIN 9     //wifi status LED pin

#define BUTTON_PIN 7  // pin the button is connected to

//those need to be char[] for some reason and cannot be #defines - forgive the C amateur...
const char TOKEN[] = "SECRET";    // secret token for identification. may not contain commas (,)
const char MESSAGE_ON[] = "SECRET,ON";   // "WiFi is ON" status message expected from server
const char MESSAGE_OFF[] = "SECRET,OFF"; // "WiFi is OFF" status message expected from server

#define STATUS_UPDATE_INTERVAL 60000 // get a status update from server every so many milliseconds

#define STATUS_VALIDITY 180000       // timeout in milliseconds after which an old status is
                                     // considered invalid and connection lost alarm is raised.
                                     // This should be greater than STATUS_UPDATE_INTERVAL

#define WIFI_TURINING_ON_BRIGHTNESS 127    // how bright the wifi status led should light to show we
                                           //   are turning ON and no confirmation has been received yet.

#define WIFI_TURINING_OFF_BRIGHTNESS 127  // how bright the wifi status led should light to show we
                                          //   are turning OFF and no confirmation has been received yet.

//log levels - don't change
#define LOG_NONE  0
#define LOG_ERR   1
#define LOG_INFO  4
#define LOG_DEBUG 6
//end of log levels

// set log level
#define LOG_LEVEL LOG_NONE

// END OF CONFIGURATION
// --------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------
//internally used variables below - don't change

RGBLed led(RED_PIN, GREEN_PIN, BLUE_PIN, COMMON_CATHODE);

#define CMD_QUERY 'q'       // command to query WiFi status from server
#define CMD_TOGGLE 't'      // command to toggle WiFi status on server

EthernetClient client;
Button button(BUTTON_PIN, INPUT_PULLUP);

unsigned long next;                    // will be used to keep track of next status refresh query
unsigned long lastStatusMillis = 0;    // last time we got a status update from the server (note that this rolls over every ca. 50 days
unsigned long currentMillis = 0;       // used to keep track of current time
unsigned long lastStatusLogMillis = 0; // keep track when the last status report was logged to serial port
#define STATUS_LOG_INTERVAL 10000      // status log interval in milliseconds

boolean wifiStatus = false;

void setup() {

  pinMode(BLUE_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);

#if LOG_LEVEL>0
  Serial.begin(9600);
#endif

  //DHCP IP assignment
  while (ethernetBegin() == false) {
    logln(LOG_INFO,F("DHCP failed. Retrying in 10s..."));
    delay(10000);
  }

  next = 0;
}

void loop() {

  currentMillis = millis();

  //keep DHCP lease
  // https://www.arduino.cc/en/Reference/EthernetMaintain - "You can call this function as often as you want", "The easiest way is to just call it on every loop()"
  //Ethernet.maintain();

  // check if status is too old and turn on alert if this is the case
  if (currentMillis - lastStatusMillis > STATUS_VALIDITY || // check if status is too old
      (currentMillis < lastStatusMillis && currentMillis > STATUS_VALIDITY)) { // or if the currentMillis rolled over and status is too old?
    if(lastStatusLogMillis + STATUS_LOG_INTERVAL > currentMillis || (currentMillis < lastStatusLogMillis && currentMillis > STATUS_LOG_INTERVAL)){
      logln(LOG_INFO,F("status is too old!"));
      logln(LOG_DEBUG,"currentMillis=" 
        + String(currentMillis)
        + ", lastStatusMillis="
        + String(lastStatusMillis)
        + ", STATUS_VALIDITY="
        + String(STATUS_VALIDITY)
        + ", that's "
        + String((currentMillis - lastStatusMillis)-STATUS_VALIDITY)
        + " overdue");
      if(client.connected()){
        logln(LOG_INFO,F("but client is still connected"));
      }
      else{
        logln(LOG_DEBUG,F("client is NOT connected"));
      }
      lastStatusLogMillis = millis();
    }
    //digitalWrite(RED_PIN, HIGH);
    led.brightness(RGBLed::RED, 50);
    //digitalWrite(BLUE_PIN, LOW);
  }

  //button press handling
  button.process();


  if(button.press()) {
    toggle();
  }
  else if (((signed long)(currentMillis - next)) > 0){
    //set next status update time
    next = currentMillis + STATUS_UPDATE_INTERVAL;
    //query status
    requestStatus();
  }


}

boolean checkConnect(){
  //networking
  if (!client.connected()) {
    logln(LOG_INFO,F("not connected"));
    if(client){
      log(LOG_INFO,"Client ready - ");
    }
    else{
      log(LOG_INFO,"Client not ready - ");
      //client.stop();
    }
    String logmsg = String("Client connecting to " + SERVER_IP + String(":") + SERVER_PORT + " - ");
    if(client.connect(SERVER_IP, SERVER_PORT)){
      logln(LOG_INFO,logmsg + "success");
      return true;
    }
    else{
      logln(LOG_ERR,logmsg + "FAILED");
      delay(1000);
      //ethernetBegin();
      return false;
    }
  }
}

void toggle(){
  if(checkConnect()){
    logln(LOG_INFO,F("Sending toggle command"));
    //send toggle command to server (will answer with status)
    client.print(TOKEN);
    client.print(",");
    client.println(CMD_TOGGLE);
  
    if(wifiStatus == false){
      //indicate we're turning on...
      led.fadeIn(RGBLed::BLUE, 5, 500);
    }
    else{
      //indicate we're turning off...
      led.fadeOut(RGBLed::BLUE, 5, 500);
    }
    requestStatus();
  }
}

void requestStatus(){
  logln(LOG_INFO,F("Querying status"));
  if(checkConnect()){
    logln(LOG_INFO,F("Connected"));
    client.print(TOKEN);
    client.print(",");
    client.println(CMD_QUERY);

    log(LOG_INFO,F("waiting..."));
    delay(100);
    logln(LOG_INFO,F("reading response"));
    

    //read response
    int size;
    size = client.available();
    if(size > 0){
    //while ((size = client.available()) > 0){
      byte msg[80];
      if (size > 80) size = 80;
      client.read(msg, size);

      // check message (token, status) and update status accordingly:
      if (strstr(msg,MESSAGE_ON) || strstr(msg,MESSAGE_OFF)) {
        log(LOG_INFO,F("Got status update"));
        log(LOG_DEBUG,": ");
        log(LOG_DEBUG,msg);
        logln(LOG_INFO,"");
        lastStatusMillis = currentMillis; // set last status update time to NOW
        digitalWrite(RED_PIN, LOW);   // clear error indicator - turn LED off
      }
      
      if (strstr((char *)msg,MESSAGE_OFF)) { // if WiFi is off
        logln(LOG_INFO,F("Wifi is off - closing connection"));
        digitalWrite(BLUE_PIN, LOW);
        wifiStatus=false;
        client.stop();
      }
      else if(strstr((char *)msg,MESSAGE_ON)){ // WiFi is on
        logln(LOG_INFO,F("Wifi is on - closing connection"));
        digitalWrite(BLUE_PIN, HIGH);
        wifiStatus=true;
        client.stop();
      }
      else{
        log(LOG_INFO,F("Received unknown status: "));
        log(LOG_INFO,(char *)msg);
        logln(LOG_INFO,F(" - keeping connection open"));
      }
      //close connection
      //client.stop();  
    }
  }
}

boolean ethernetBegin(){
  if (Ethernet.linkStatus() == Unknown) {
    logln(LOG_INFO,"Link status unknown. Link status detection is only available with W5200 and W5500.");
  }
  else if (Ethernet.linkStatus() == LinkON) {
    logln(LOG_INFO,"Link status: On");
  }
  else if (Ethernet.linkStatus() == LinkOFF) {
    logln(LOG_INFO,"Link status: Off");
  }
  
  logln(LOG_INFO,"Initialize Ethernet with DHCP:");

  //DHCP IP assignment
  if (Ethernet.begin(mac) == 0) {
    logln(LOG_INFO,"FAILED");
    return false;
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);

  logln(LOG_INFO,"localIP: " + Ethernet.localIP());
  logln(LOG_INFO,"subnetMask: " + Ethernet.subnetMask());
  logln(LOG_INFO,"gatewayIP: " + Ethernet.gatewayIP());
  logln(LOG_INFO,"dnsServerIP: " + Ethernet.dnsServerIP());
  return true;
}

bool log(byte severity, String message){
#if LOG_LEVEL>0
  if(Serial && Serial.availableForWrite()>=message.length()+1 && LOG_LEVEL >= severity){
    Serial.print(message);
    return true;
  }
#endif
  return false;
}

bool logln(byte severity, String message){
  return log(severity, message+'\n');
}
