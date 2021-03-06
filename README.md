# wifibutton
Ethernet connected physical wifi button for OpenWRT using Arduino and ENC28J60 or official Arduino Ethernet shield or WIZnet W5x00 adapter

## What is this about?
If you want to switch on and off the wifi on your OpenWRT router/access point but:
* do not have a switch on the router
* the router is located at an difficult to access location
* you want to have the button in a different location than the router
Then, this might be for you.

It's a button and status LED connected to an Arduino, which connects to your LAN by means of an ethernet adapter. It uses the LAN to connect to your router on which the wifitoggler.sh is constantly running.
Pressing the button will send a toggle command to the wifitoggler script running on the router, which will turn on or off wifi.
The Arduino is regularly querying the state of the wifi on the router and uses the LED to show the same.
If the last status update is too long ago, the Arduino will start and keep blinking the LED.

## What is this NOT about?
* If you are the "wifi always on" guy, you are probably not interested in this
* If you are using an off-the-shelf router not running OpenWRT (or similar), this will not work
* If you want to switch wifi on or off from a location where there is no Ethernet

## Hardware setup
### Hardware required to build
  * Arduino
  * ENC28J60 or W5x00 based ethernet adapter.
  Note that for the official Arduino ethernet shields or other W5100 based adapters, you need to switch the library
  * Push button
  * 2 LEDs with resistors (1 RGB LED built into the push button would be great)

### Schematic

                                                              +-----+
                                 +----[PWR]-------------------| USB |--+
                                 |                            +-----+  |
                                 |         GND/RST2  [ ][ ]            |
                                 |       MOSI2/SCK2  [ ][ ]  A5/SCL[ ] |   C5
                                 |          5V/MISO2 [ ][ ]  A4/SDA[ ] |   C4
                                 |                             AREF[ ] |
                                 |                              GND[ ] |      --> GND to button contact 1
                                 | [ ]N/C                    SCK/13[ ] |   B5 --> SCK on Ethernet module
                                 | [ ]IOREF                 MISO/12[ ] |   .  --> SO on Ethernet module
                                 | [ ]RST                   MOSI/11[ ]~|   .  --> SI on Ethernet module
      VCC on Ethernet module <-- | [ ]3V3    +---+               10[ ]~|   .  --> CS on Ethernet module
                                 | [ ]5v    -| A |-               9[ ]~|   .  --> button blue LED
      GND on Ethernet module <-- | [ ]GND   -| R |-               8[ ] |   B0
                  GND on LED <-- | [ ]GND   -| D |-                    |
                                 | [ ]Vin   -| U |-               7[ ] |   D7 --> button contact 2
                                 | [ ]A0    -| N |-               6[ ]~|   .  --> button red LED
                                 |          -| I |-               5[ ]~|   .  --> button green LED (unused)
                                 | [ ]A1    -| O |-               4[ ] |   .
                                 | [ ]A2     +---+           INT1/3[ ]~|   .
                                 | [ ]A3                     INT0/2[ ] |   .
                                 | [ ]A4/SDA  RST SCK MISO     TX>1[ ] |   .
                                 | [ ]A5/SCL  [ ] [ ] [ ]      RX<0[ ] |   D0
                                 |            [ ] [ ] [ ]              |
                                 |  UNO_R3    GND MOSI 5V  ____________/
                                  \_______________________/

                                   http://busyducks.com/ascii-art-arduinos

                          connection of ENC28J60 ethernet module according to
                              https://www.arduino.cc/en/Reference/Ethernet

           ________
          (   OO   )
        +------------+   BLUE+   +------+
        |   Button   |-----------| 220Ω |--> 9 on Arduino
        |      +     |           +------+
        |            |   RED+    +------+
        |   RGB LED  |-----------| 220Ω |--> 6 on Arduino
        |            |           +------+
        |            |   GREEN+  +------+
        |            |-----------| 220Ω |--> 5 on Arduino
        +------------+           +------+
            2| |1        GND
             | +--------------------------> GND on Arduino
             |
             |        button press
             +----------------------------> PIN 7 on Arduino

             normally open switch, wiring and configuration converts it to a virtual normally closed switch
             (pulled up by Arduino-internal resistor, pull down on press)

             BLUE LED on = WiFi on, LED off = WiFi off
             RED LED on  = no connection to server / no status from server


      Arduino +-----------------------+
              | [ ] [ ] CLK/NT    +------+
         /12  | [ ] [ ] WOL/SO    | ETH  |
       11/13  | [ ] [ ] SI/SCK    |      |
       10/    | [ ] [ ] CS/RST    | RJ45 |
       3V3/GND| [ ] [ ] VCC/GND   +------+
              +-----------------------+
                HanRun ENC28J60 module
                - WIZnet W5x00 module similar

## Software setup

### Libraries required:
  * UIPEthernet.h: https://github.com/UIPEthernet/UIPEthernet if using ENC28J60 Ethernet module
      - Considering switching to https://github.com/dimitar-kunchev/UIPEthernet because it supports non-blocking connect).
  * Ethernet.h: Arduino Ethernet (https://www.arduino.cc/en/Reference/Ethernet) library if using a W5x00 based adapter
  * Button.h: https://github.com/leethomason/Button (forked from classic https://github.com/t3db0t/Button)
  * RGBLED.h: https://github.com/wilmouths/RGBLed

### Arduino sketch
  * Modify to suit your needs, particularly:
    * serverIp
    * serverPort
    * TOKEN, MESSAGE_ON and MESSAGE_OFF
    * WIFI_LED_PIN
    * ERR_LED_PIN
    * BUTTON_PIN
  * Upload to your Arduino
### wifitoggler.sh
  * modify to your needs, particularly:
    * LISTEN_PORT (needs to match the serverPort in the Arduino sketch)
    * WIFI_DEVICE (needs to match your router, execute `uci export wireless` on your router to find out)
    * SECRET (needs to match the secret in TOKEN, MESSAGE_ON and MESSAGE_OFF in the Arduino sketch)
  * Transfer the script to your router
  * ensure you have socat installed on the router
  * start script `/path/to/wifitoggler.sh`
  It will go to the background and only log debug output to the console
  * ensure the script is started at reboot (OpenWRT LuCI web interface)
