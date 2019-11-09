# WirelessPalaControl

This project use a D1 Mini to control Palazzetti/Fumis stove using :
 - HTTP GET requests

D1 mini is cabled to the stove using a special adapter.

It communicates using Serial protocol on ESP8266 alternative pins : D7(GPIO13) as RX / D8(GPIO15) as TX


## Build your adapter

TODO

## Build your WirelessPalaControl

All files are inside schematic subfolder and has been designed with KiCad

### Schematic

TODO

### PCB

TODO

### Code/Compile

Source code can be compiled using VisualStudioCode/Platformio and flashed onto a D1 Mini

### Print your box

Box project (Fusion 360) can be found into `box` folder

TODO

## Run

### First Boot

During First Boot, the ESP boot in Access Point Mode

- Network SSID : `WirelessPalaControlXXXX`
- Password : `PasswordPalaControl`
- ESP IP : `192.168.4.1`

Connect to this network and then configure it.

### Configuration

WirelessPalaControl offers you some webpages in order to configure it :

- `Status` return you the current status of the module :

TODO

![status screenshot](https://raw.github.com/Domochip/WirelessDS18B20/master/img/status.png)

- `Config` allows you to change configuration :

TODO

![config screenshot](https://raw.github.com/Domochip/WirelessDS18B20/master/img/config.png)

- **ssid & password** : IDs of your Wifi Network
- **hostname** : name of ESP on the network
- **IP,GW,NetMask,DNS1&2** : Fixed IP configuration

- `Firmware` allows you to flash a new firmware version :

![firmware screenshot](https://raw.github.com/Domochip/WirelessDS18B20/master/img/firmware.png)

- `Discover` allows you to find all DomoChip devices on your network :

![discover screenshot](https://raw.github.com/Domochip/WirelessDS18B20/master/img/discover.png)

## Use it

### Basics

Usage (answers are in JSON format):

- `http://{IP}/cgi-bin/sendmsg.lua?cmd=GET+TMPS` will return temperatures of the stove
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=GET+FAND` will return Fan values
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=GET+SETP` will return current Set Point (desired temperature)
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=GET+POWR` will return current power (1-5)
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=GET+CUNT` will return some counters
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=GET+CNTR` will return some counters (same as GET+CUNT)
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=GET+PARM+92` will return parameter (ex : 92=pellet type (1-3))
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=GET+HPAR+57` will return hidden parameter (ex : 57=%of pellet to feed for pellet type 3)

- `http://{IP}/cgi-bin/sendmsg.lua?cmd=CMD+ON` will turn stove ON
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=CMD+OFF` will turn stove OFF
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=SET+POWR+3` will set power (1-5)
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=SET+SETP+20` will set Set Point (desired temperature)
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=SET+STPF+19.8` will set Set Point with a 0.2° precision (depend of your stove model)
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=SET+RFAN+7` will set Room Fan value (0-5;6=Max;7=Auto)
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=SET+FN3L+0` will set Room Fan 3 value (0-5)
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=SET+FN4L+0` will set Room Fan 4 value (0-5)
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=SET+PARM+92+2` will set parameter 92 to value 2 (ex : 92=pellet type (1-3))
- `http://{IP}/cgi-bin/sendmsg.lua?cmd=GET+HPAR+57+95` will set hidden parameter 57 to value 95 (ex : 57=%of pellet to feed for pellet type 3)
