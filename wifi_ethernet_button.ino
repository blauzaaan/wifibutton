/*

 ***********************
 * Arduino WiFi button *
 ***********************

 -----------------------------------------------------------------------

  Copyright 2017 by blauzaaan, https://github.com/blauzaaan

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

  Adjust to your needs, particularly:
    * serverIp
    * serverPort
    * WIFI_LED_PIN
    * BUTTON_PIN
    * TOKEN, MESSAGE_ON and MESSAGE_OFF (they should all use the same secret, unless you know what you are doing)
*/

#include <UIPEthernet.h>
#include <Button.h>

// CONFIGURATION
// adjust these to your needs

IPAddress serverIp = IPAddress(192, 168, 1, 1);
const short serverPort = 5015;

// don't use onboard LED as pin 13 is used by ethernet adapter!
const byte WIFI_LED_PIN = 9;      //wifi status LED pin
const byte ERR_LED_PIN = 6;      //communication error indicator LED pin
const byte BUTTON_PIN = 7;  // pin the button is connected to

char TOKEN[] = "SECRET";    // secret token for identification. may not contain commas (,)
const char CMD_QUERY = 'q';       // command to query WiFi status from server
const char CMD_TOGGLE = 't';      // command to toggle WiFi status on server
const char MESSAGE_ON[] = "SECRET,ON";   // "WiFi is ON" status message expected from server
const char MESSAGE_OFF[] = "SECRET,OFF"; // "WiFi is OFF" status message expected from server

const unsigned long STATUS_VALIDITY = 180000;       // timeout in milliseconds after which an old status is considered invalid and connection lost alarm is raised
                                                   //   this should be greater than STATUS_UPDATE_INTERVAL
const unsigned long STATUS_UPDATE_INTERVAL = 60000; // get a status update from server every so many milliseconds

const byte WIFI_TURINING_ON_BRIGHTNESS = 127;    // how bright the wifi status led should light to show we
                                                //   are turning ON and no confirmation has been received yet.

const byte WIFI_TURINING_OFF_BRIGHTNESS = 127;  // how bright the wifi status led should light to show we
                                                //   are turning OFF and no confirmation has been received yet.

// END OF CONFIGURATION

//internally used variables below - don't change
EthernetClient client;
Button button(BUTTON_PIN, INPUT_PULLUP);

unsigned long next;                 // will be used to keep track of next status refresh query
unsigned long lastStatusMillis = 0;                // last time we got a status update from the server (note that this rolls over every ca. 50 days
unsigned long currentMillis = 0;  // used to keep track of current time

boolean wifiStatus = false;

void setup() {

  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(ERR_LED_PIN, OUTPUT);

  Serial.begin(9600);

  //randomly generated MAC address
  uint8_t mac[6] = { 0x80, 0x41, 0x9C, 0xDA, 0x64, 0xCE };

  //DHCP IP assignment
  Ethernet.begin(mac);

  Serial.print("localIP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("subnetMask: ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("gatewayIP: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("dnsServerIP: ");
  Serial.println(Ethernet.dnsServerIP());

  next = 0;
}

void loop() {

  currentMillis = millis();

  // check if status is too old and turn on alert if this is the case
  if (currentMillis - lastStatusMillis > STATUS_VALIDITY || // check if status is too old
      (currentMillis < lastStatusMillis && currentMillis > STATUS_VALIDITY)) { // or if the currentMillis rolled over and status is too old?
    Serial.println("status is too old!");
    Serial.println("currentMillis="+String(currentMillis));
    Serial.println("lastStatusMillis="+String(lastStatusMillis));
    Serial.println("STATUS_VALIDITY="+String(STATUS_VALIDITY));
    digitalWrite(ERR_LED_PIN, HIGH);
    digitalWrite(WIFI_LED_PIN, LOW);
  }

  //button press handling
  button.process();

  //networking
  if (!client.connected()) {
    Serial.print("Client connecting to ");
    Serial.print(serverIp);
    Serial.print(":");
    Serial.println(serverPort);
    client.connect(serverIp, serverPort);
  }

  if (client.connected()) {
    if(button.press()) {
      if(wifiStatus == false){
        //indicate we're turning on...
        analogWrite(WIFI_LED_PIN, WIFI_TURINING_ON_BRIGHTNESS);
      }
      else{
        //indicate we're turning off...
        analogWrite(WIFI_LED_PIN, WIFI_TURINING_OFF_BRIGHTNESS);
      }
      Serial.println("Sending toggle command");
      //send toggle command to server (will answer with status)
      client.print(TOKEN);
      client.print(",");
      client.println(CMD_TOGGLE);
    }
    else if (((signed long)(currentMillis - next)) > 0){
      //set next status update time
      next = currentMillis + STATUS_UPDATE_INTERVAL;
      //query status
      Serial.println("Querying status");
      client.print(TOKEN);
      client.print(",");
      client.println(CMD_QUERY);
    }

    //read response
    int size;
    while ((size = client.available()) > 0) {
      uint8_t* msg = (uint8_t*)malloc(size-1);
      size = client.read(msg, size);
      //Serial.write(msg, size);

      // check message (token, status) and update status accordingly:
      if (strstr((char *)msg,MESSAGE_ON) || strstr((char *)msg,MESSAGE_OFF)) {
        Serial.print("Got status update: ");
        Serial.write(msg,size);
        lastStatusMillis = currentMillis; // set last status update time to NOW
        digitalWrite(ERR_LED_PIN, LOW);   // clear error indicator - turn LED off
      }

      if (strstr((char *)msg,MESSAGE_OFF)) { // if WiFi is off
        Serial.println("Wifi is off");
        digitalWrite(WIFI_LED_PIN, LOW);
        wifiStatus=false;
      }
      else if(strstr((char *)msg,MESSAGE_ON)){ // WiFi is on
        Serial.println("Wifi is on");
        digitalWrite(WIFI_LED_PIN, HIGH);
        wifiStatus=true;
      }
      else{
        Serial.print("Received unknown status: ");
        Serial.println((char *)msg);
      }

      free(msg);
    }
  }
  else
    Serial.println("Client connect failed");
}

