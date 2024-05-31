# Linear Actuator
## Overview
Arduino sketch for an ESP8266 to control a linear actuator with thermostat control. 

**Features:**
   - Web interface with status monitoring, override controls, and temperature setpoint adjustments
   - Fallback to automatic thermostat control if WiFi setup fails
   - Analog temperature sensor with moving average
   - Limit switches and motor timeouts for safety

## Installation
### Manual Installation
_Use this option if compiling from source code._
1. Download the [Arduino IDE](https://www.arduino.cc/en/software)
2. Go to File > Preferences, and add the following to the _Additional boards manager URLs_ field: `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. Go to Tools > Board > Boards Manager, and install _esp8266 by ESP8266 Community_
3. Download/clone this repository
4. Install the following packages:
   - ESPAsyncTCP
   - ESPAsyncWebServer
   - ElegantOTA
   - [ESPAsyncWiFiManager](https://github.com/alanswx/ESPAsyncWiFiManager/tree/master)
5. Compile the sketch and upload the sketch to the ESP8266

### Firmware Update
_Use this option if the controller is running and needs to be updated._
Obtain a binary (\*.bin) file from someone who knows how to make one. Click the Update button in the footer and upload the file.

## WiFi Setup
If the controller cannot connect to its last known WiFi network, it will create its own temporary access point starting with "AP_". Connect to this acess point with the password "greenhouse", then enter your WiFi credentials. The onboard blue LED indicates the connection status:
   - Solid: Connected to WiFi
   - Blinking at ~1 Hz: Awaiting credentials through access point
   - Blinking at ~5 Hz: Access point timed out; proceeding with automatic temperature control

Once connected, the controller will print its IP address over serial. Navigate to this address, e.g.,  "http://192.168.0.191/" on any browse and bookmark the page.

In your router settings, set the IP address as static. Otherwise the address could change.

The WiFi credentials can be reset by pressing and holding both limit switches while rebooting the controller.

## Hardware
   - Connect the ESP8266 to a micro-USB power supply.
   - Connect a 12V power supply to the barrel jack connector.
   - Motor connector (2 wire)*: 
      - Left = red
      - Right = black
   - Limit switch connector (3 wire)*: 
      - Left = Normally Open (NO) for window closed switch
      - Middle = Common (C) for both switches
      - Right = Normally Open (NO) for window open switch

_*Wire ferrules are strongly recommended_