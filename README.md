# WirelessPalaControl

This project uses "palazzetti library" and a D1 Mini to control Fumis based stove using :
 - HTTP GET requests
 - MQTT

D1 mini is cabled to the stove using an adapter board.  
It communicates using Serial protocol on ESP8266 alternative pins : D7(GPIO13) as RX / D8(GPIO15) as TX

## Compatibility

It appears that Fumis Controller is used by those brands for their stoves : 

* Palazzetti (All)
* Jotul (tested successfully on PF 620 & 720)
* TurboFonte
* Godin
* Fonte Flamme
* Invicta
* Alpis
* Faizen
* HETA

If you have this controller in your stove, it's likely to be compatible.  
![Fumis Controller](https://raw.github.com/Domochip/WirelessPalaControl/master/img/fumis.png)

## Build your adapter

You can use this adapter with:
 - a D1 Mini to build a WirelessPalaControl
 - another controller with serial interface
 - an USB-Serial adapter and a computer to monitor or upgrade your stove

It is designed by Palazzetti using a Si8621 (Silicon Labs Isolator) to provide electrical isolation between uController and Stove electronic.  
This design uses exact same schematic and components.


All files are inside `schematic` subfolder and has been designed with KiCad (free and open source)

### Schematic

![WirelessPalaControl schematic](https://raw.github.com/Domochip/WirelessPalaControl/master/img/schematic.png)

### PCB

![WirelessPalaControl PCB](https://raw.github.com/Domochip/WirelessPalaControl/master/img/pcb-top.png)![WirelessPalaControl PCB2](https://raw.github.com/Domochip/WirelessPalaControl/master/img/pcb-bottom.png)

![WirelessPalaControl 3boards](https://raw.github.com/Domochip/WirelessPalaControl/master/img/3boards.png)

*We produced a small batch of this adapter for test/debugging and our personal use.
If you are interested, please PM.*

### Print your box

Box project (Fusion 360 & STL) can be found into `box` folder

![WirelessPalaControl box](https://raw.github.com/Domochip/WirelessPalaControl/master/img/box.png)

### Code/Compile/Flash

Source code can be compiled using VisualStudioCode/Platformio and flashed onto a D1 Mini  
Or  
Download latest release in Release section

### Connect

⚠️ **You need to use a crossed RJ11 phone cable like this:** ⚠️  
![WirelessPalaControl rj11](https://raw.github.com/Domochip/WirelessPalaControl/master/img/rj11-pinout.png)

Most of stove have an RJ11/RJ12 connector for PalaControl connection.  
If you don't have it, you need to cable it using a splitter to connect screen and PalaControl at the same time :  
![WirelessPalaControl cabling](https://raw.github.com/Domochip/WirelessPalaControl/master/img/cabling.png)

Splitter and additional cable can be found on Aliexpress (search for "6p6c splitter" and "rj12 cable").  
The splitter should correspond to this small schematic :  
![WirelessPalaControl schematic-splitter](https://raw.github.com/Domochip/WirelessPalaControl/master/img/schematic-splitter.png)

## Run

### First Boot

During First Boot, the ESP boot in Access Point Mode

- Network SSID : `WirelessPalaControlXXXX`
- Password : `PasswordPalaControl`
- ESP URL : 👉 http://wpalacontrol.local 👈

Connect to this network and then configure it.

### Configuration

WirelessPalaControl offers you some webpages in order to configure it:

#### Status

It returns you useful informations about the module but also regarding the stove:  
![status screenshot](https://raw.github.com/Domochip/WirelessPalaControl/master/img/status.png)  
**Then 1 minute later, other stove information appears (default upload period)**
![status2 screenshot](https://raw.github.com/Domochip/WirelessPalaControl/master/img/status2.png)

#### Config

It allows you to change configuration:  
![config screenshot](https://raw.github.com/Domochip/WirelessPalaControl/master/img/config.png)  
  **ssid & password** : IDs of your Wifi Network  
  **hostname** : name of ESP on the network  
  **IP,GW,NetMask,DNS1&2** : Fixed IP configuration  
![configMQTT screenshot](https://raw.github.com/Domochip/WirelessPalaControl/master/img/configMQTT.png)  
  Fill-in MQTT broker information

#### Firmware

It allows you to flash a new firmware version:  
![firmware screenshot](https://raw.github.com/Domochip/WirelessPalaControl/master/img/firmware.png)

#### Discover

It allows you to find all DomoChip devices on your network:  
![discover screenshot](https://raw.github.com/Domochip/WirelessPalaControl/master/img/discover.png)

## Use it

### HTTP

Natively, HTTP GET request can be sent directly to the module.  
Syntax:  **http://wpalacontrol.local/cgi-bin/sendmsg.lua?cmd={command}**

### MQTT

Command can be sent via MQTT to %BaseTopic%**/cmd** topic once MQTT is configured.  
Execution result is:  
 - received back on %BaseTopic%**/result** in JSON format 
 - published following the configured MQTT Type

### Command List

- `GET+STDT`: get static data
- `GET+ALLS`: get all status data
- `GET+STAT`: get status of the stove⏲️
- `GET+TMPS`: get temperatures of the stove⏲️
- `GET+FAND`: get Fan values⏲️
- `GET+SETP`: get current Set Point (desired temperature)⏲️
- `GET+POWR`: get current power (1-5)⏲️
- `GET+CUNT`: get some counters
- `GET+CNTR`: get some counters (same as GET+CUNT)⏲️
- `GET+DPRS`: get delta pressure data⏲️
- `GET+TIME`: get stove clock data⏲️
- `GET+IOPT`: get IO ports status
- `GET+SERN`: get stove Serial Number
- `GET+MDVE`: get stove model and fw version
- `GET+CHRD`: get chrono data
- `GET+PARM+92`: get parameter (ex : 92=pellet type (1-3))
- `GET+HPAR+57`: get hidden parameter (ex : 57=% of pellet to feed for pellet type 3)
- `BKP+PARM+CSV`: get all parameters in a CSV file (HTTP only) 🔷
- `BKP+PARM+JSON`: get all parameters in a JSON file (HTTP only) 🔷
- `BKP+HPAR+CSV`: get all hidden parameters in a CSV file (HTTP only) 🔷
- `BKP+HPAR+JSON`: get all hidden parameters in a JSON file (HTTP only) 🔷
- `CMD+ON`: turn stove ON
- `CMD+OFF`: turn stove OFF
- `SET+POWR+3`: set power (1-5)
- `SET+PWRU`: increase power by 1 unit
- `SET+PWRD`: decrease power by 1 unit
- `SET+SETP+20`: set Set Point (desired temperature)
- `SET+STPU`: increase Set Point by 1 unit
- `SET+STPD`: decrease Set Point by 1 unit
- `SET+RFAN+7`: set Room Fan value (0-5;6=Max;7=Auto)
- `SET+FN2U`: increase Room Fan by 1 unit
- `SET+FN2D`: decrease Room Fan by 1 unit
- `SET+FN3L+0`: set Room Fan 3 value (0-5)
- `SET+FN4L+0`: set Room Fan 4 value (0-5)
- `SET+SLNT+0`: set Silent mode value (0-1)
- `SET+TIME+2023-12-28+19:42:00`: set stove Date and Time (2000-2099) (1-12) (1-31) (0-23) (0-59) (0-59)
- `SET+CSST+0`: set Chrono Status value (0-1)
- `SET+CSTH+2+18`: set Chrono Program Start Hour (1-6) (0-23)
- `SET+CSTM+2+30`: set Chrono Program Start Minute (1-6) (0-59)
- `SET+CSPH+2+22`: set Chrono Program Stop Hour (1-6) (0-23)
- `SET+CSPM+2+45`: set Chrono Program Stop Minute (1-6) (0-59)
- `SET+CSET+2+19`: set Chrono Program Set Point (1-6) (desired temperature)
- `SET+CDAY+7+3+6`: set Chrono Program for week day (Day-Mem-Prog) (1-7) (1-3) (1-6)
- `SET+CPRD+1+19+18+30+22+45`: set Chrono Program data (Prog-Temp-StartH-StartM-StopH-StopM) (1-6) (temperature) (0-23) (0-59) (0-23) (0-59)
- `SET+PARM+92+2`: set parameter 92 to value 2 (ex : 92=pellet type (1-3))
- `SET+HPAR+57+95`: set hidden parameter 57 to value 95 (ex : 57=% of pellet to feed for pellet type 3)

- `SET+STPF+19.8`: set Set Point with a 0.2° precision (depend of your stove model)🔷

⏲️: Published automatically if "Upload Period" is configured
🔷: WPalaControl specific commands


### Description

MQTT infos published every "Upload Period":
- `STATUS`: status of the stove
- `LSTATUS`: status of the stove
- `T1`, `T2`, `T3`, `T4`, `T5`: temperature of the stove
- `F1V`, `F2V`, `F2L`, `F2LF`, `F3L`, `F4L`: fan values (meaning depend of your stove model)
- `IGN`: ignition counter
- `IGNERRORS`: ignition error counter
- `POWERTIME`: total heating time (hour:minute)
- `HEATTIME`: ??? (hour:minute)
- `SERVICETIME`: heating time since last maintenance (hour:minute)
- `ONTIME`: time from last power ON (hour:minute)
- `OVERTMPERRORS`: overtemperature error counter
- `STOVE_DATETIME`: date of the stove
- `STOVE_WDAY`: week day of the stove
- `SETP`: current Set Point (desired temperature)
- `PQT`: wood pellet consumption
- `PWR`: current power (1-5)
- `FDR`: feeder
- `DP_TARGET`: delta pressure target
- `DP_PRESS`: actual delta pressure