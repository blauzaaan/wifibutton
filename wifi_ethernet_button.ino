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
    * LEDPIN
    * BUTTON_PIN
    * TOKEN, MESSAGE_ON and MESSAGE_OFF (they should all use the same secret, unless you know what you are doing)
*/

#include <UIPEthernet.h>
#include <Button.h>

IPAddress serverIp = IPAddress(192, 168, 1, 1);
const short serverPort = 5015;

const byte LEDPIN = 8;      // don't use onboard LED as pin 13 is used by ethernet adapter!
const byte BUTTON_PIN = 6;  // pin the button is connected to

char TOKEN[] = "SECRET";    // secret token for identification. may not contain commas (,)
const char CMD_QUERY = 'q';       // command to query WiFi status from server
const char CMD_TOGGLE = 't';      // command to toggle WiFi status on server
const char MESSAGE_ON[] = "SECRET,ON";   // "WiFi is ON" status message expected from server
const char MESSAGE_OFF[] = "SECRET,OFF"; // "WiFi is OFF" status message expected from server

EthernetClient client;
Button button(BUTTON_PIN, INPUT_PULLUP);

unsigned long lastStatusMillis = 0;                // last time we got a status update from the server (note that this rolls over every ca. 50 days
const unsigned long STATUS_VALIDITY = 180000;       // timeout in milliseconds after which an old status is considered invalid and connection lost alarm is raised
                                                   //   this should be greater than STATUS_UPDATE_INTERVAL
const unsigned long STATUS_UPDATE_INTERVAL = 60000; // get a status update from server every so many milliseconds

boolean ledState = LOW;
unsigned long previousBlinkMillis = 0;             // last time we toggled the led when blinking
const unsigned int BLINK_INTERVAL = 300;           // blink interval in milliseconds
boolean blinking = true;                           // are we in blinking mode?

unsigned long currentMillis = 0;  // used to keep track of current time
unsigned long next;                 // will be used to keep track of next status refresh query

void setup() {

  pinMode(LEDPIN, OUTPUT);

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
    if (blinking == false) {
      Serial.println("status is too old!");
      Serial.println("currentMillis="+String(currentMillis));
      Serial.println("lastStatusMillis="+String(lastStatusMillis));
      Serial.println("STATUS_VALIDITY="+String(STATUS_VALIDITY));
      blinking = true;
    }
  }

  // if in alert mode, blink
  if (blinking && currentMillis - previousBlinkMillis >= BLINK_INTERVAL) {
    // save the last time you blinked the LED
    previousBlinkMillis = currentMillis;

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(LEDPIN, ledState);
  }

  //button press handling
  button.process();

  //networking
  if (((signed long)(currentMillis - next)) > 0 || true) {
    if (!client.connected()) {
      Serial.print("Client connecting to ");
      Serial.print(serverIp);
      Serial.print(":");
      Serial.println(serverPort);
      client.connect(serverIp, serverPort);
    }

    if (client.connected()) {
      if(button.press()) {
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
          blinking = false; // stop blinking (just in case we already started alarm)
          ledState = LOW; // reset LED state for next alert sequence
        }

        if (strstr((char *)msg,MESSAGE_OFF)) { // if WiFi is off
          Serial.println("Wifi is off");
          digitalWrite(LEDPIN, LOW);
        }
        else if(strstr((char *)msg,MESSAGE_ON)){ // WiFi is on
          Serial.println("Wifi is on");
          digitalWrite(LEDPIN, HIGH);
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
}
