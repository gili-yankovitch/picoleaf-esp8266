# Picoleaf-ESP8266
This is an ESP8266-based Nanoleaf-like project. Tested with the ESP-01 board, this flashes the SoC flash and uses its storage as configuration storage for WiFi connectivity.

## Build and Flash
The project uses the https://github.com/plerup/makeEspArduino build system, mainly tested on Linux. Download, and install the build environment. Then, connect the board and configure the Makefile to point to the relevant `/dev/ttyUSBXX` device and build:
```
# Build BSP, environment and code
make

# Flash the device
make flash
```

## Installation
On first use, the board will expose a new WiFi network called `Picoleaf`. Its IP address should be `192.168.4.1`. Connect to this access-point and logging to that IP address, the device serves a setup page.  

<p align="center">
  <img src="picoleaf-esp8266-setup.jpg">
</p>

This setup page asks you to choose your own WiFi SSID and supply its password. The last configuration field is the management URL the board should contact to get its animation and configuration. This will usually look something like: `http://<YOUR-HOSTNAME>/get`. Setting up this management interface is done by deploying this sister-repo: https://github.com/gili-yankovitch/PicoLeaf

## Electronics
* AP102 - This uses `APA102` Addressable LEDs and a driver is implemented here to use them. Though labeled for 5v usage, it is possible to operate them using the 3.3v input voltage the ESP8266 receives as well as 3.3v TTL logic GPIOs.
* BOB-12009 - For 5v step-up, there's a really good one labeled `BOB-12009` that can be of use, though not really needed.
* AS2850 - Powering this directly from USB supplying 5v input voltage, the `AS2850` is used as voltage regulator for the ESP8266 operating at 3.3v

## 3D Printables
Under `3d-models` are the 3d-printable models used for this build. For each triangle, print 3 cases, 3 back covers and 3 long connectors. Print parameters:
```
Layer height: 0.2mm
Infill: 5%
Nozzle size: 0.4mm 
Support: Enabled
```
