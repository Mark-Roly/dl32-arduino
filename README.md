<a id="readme-top"></a>
  <h1 align="center">DL32-S3</h1>
  <p align="center">
    The ESP32 powered smart door lock controller
    <br />
  </p>

<!-- ABOUT THE PROJECT -->
## About The Project

TBC
<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- GETTING STARTED -->
## Getting Started

Since the controller is capable of running offline, no configuration file is required unless you intend to take advantage of online features such as web interface, remote unlock/monitoring, mqtt, ftp, OTA Updates, etc

### Preparing the SD card with configuration file dl32.json
The easiest way to perform initial configuration, is by transferring your configuration file to the unit using an SD card. Create a text file on a FAT formatted SD card and name the file dl32.json (case sensitive).

Use the below sample dl32.json configuration as a base:
 ```   
// Configuration file for DL32-S3
// https://github.com/Mark-Roly/dl32-arduino
// Minimum firmware revision 20250424
{
    "wifi_enabled": false,
    "wifi_ssid": "wifissid",
    "wifi_password": "wifipass",
    "mqtt_enabled": false,
    "mqtt_server": "192.168.1.123",
    "mqtt_topic": "DL32",
    "mqtt_client_name": "DL32",
    "mqtt_auth": false,
    "mqtt_user": "mqttuser",
    "mqtt_password": "mqttpass",
    "ftp_enabled": false,
    "ftp_user": "ftpuser",
    "ftp_password": "ftppass",
    "exitUnlockDur_S": 5,
    "httpUnlockDur_S": 5,
    "keyUnlockDur_S": 5,
    "cmndUnlockDur_S": 5,
    "badKeyLockoutDur_S": 2,
    "exitButMinThresh_Ms": 50,
    "keyWaitDur_Ms": 1500,
    "garageMomentaryDur_Ms": 500,
    "addKeyTimeoutDur_S": 10
}
```

See descriptions of each parameter below:

 - TBC

### Flashing the configuration to the unit
Insert the SD card into the slot
With the unit powered on, gold the button labeled AUX for 5 seconds until you hear 5 short beeps and the blue LED turns purple.
Release the AUX button.
A successful config flash will result in a *twinkle* sound
<p align="right">(<a href="#readme-top">back to top</a>)</p>
