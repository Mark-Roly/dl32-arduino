<a id="readme-top"></a>
  <h1 align="center">DL32-S3</h1>
  <p align="center">
    The ESP32 powered smart door lock controller
    <br />
  </p>

<!-- ABOUT THE PROJECT -->
## About The Project
An ongoing effort to create the perfect smart lock.<br/>
In short, the DL32-S3 is a smart lock controller that can be interacted with via RFID, keypad, MQTT, HTTP, FTP, Serial, and supports a varierty of accessories.
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
    "mqtt_tls": false,
    "mqtt_auth": false,
    "mqtt_user": "mqttuser",
    "mqtt_password": "mqttpass",
    "magsr_present" : true,
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

- **[param name]** ([datatype], [default val]) [description]
```
- wifi_enabled (boolean, false) Dictates whether or not the onboard mqtt client is enabled 
- wifi_ssid (string) SSID for wifi network connection
- wifi_password (string) Password for wifi network connection
- mqtt_enabled (boolean, false) Dictates whether or not the onboard mqtt client is enabled 
- mqtt_server (string) Hostname or IP address of remote mqtt broker
- mqtt_topic (string, "dl32s3") mqtt topic base to use for sub topics
- mqtt_client_name (string, "dl32s3") Client name to use for mqtt connection
- mqtt_tls (boolean, false) Dictates whether connection to the broker uses tls private key authentication (Disregards mqtt_auth, mqtt_user and mqtt_password when true. Requires 3x cert/key files. See doc)
- mqtt_auth (boolean, false) Dictates whether username/password credentials should be used for mqtt connection. Not valid when mqtt_tls is enabled.
- mqtt_user (string) Username for mqtt broker connection. Not valid when mqtt_tls is enabled.
- mqtt_password (string) Password for mqtt broker connection. Not valid when mqtt_tls is enabled.
- magsr_present (boolean, false) Dictates whether or not a connected magnetic door sensor should be used 
- ftp_enabled (boolean, false) Dictates whether or not the onboard ftp service is enabled 
- ftp_user (string) Username for ftp connections
- ftp_password (string) Password for ftp connections
- exitUnlockDur_S (integer, 5) Time in seconds that the lock will remain unlocked for when exit button is pressed
- httpUnlockDur_S (integer, 5) Time in seconds that the lock will remain unlocked for when WebUI unlock is performed
- keyUnlockDur_S (integer, 5)  Time in seconds that the lock will remain unlocked for when valid RFID key is read
- cmndUnlockDur_S (integer, 5) Time in seconds that the lock will remain unlocked for when url/mqtt/serial unlock command is sent
- badKeyLockoutDur_S (integer, 5) Time in seconds that the unit will turn unresponsive for with the red led illuminated when an unauthorized key is read
- exitButMinThresh_Ms (integer, 50) Minimum time in miliseconds that the exit button must be pressed for to register an unlock
- keyWaitDur_Ms (integer, 1500) Timeout in miliseconds between keypad digits eg: max time between button presses for keypad input
- garageMomentaryDur_Ms (integer, 500) Time in miliseconds that the momentary signal will be on when in garage mode
- addKeyTimeoutDur_S (integer, 10) Time in seconds that the unit will await a new card when in Key Add Mode before timing out
```

### Flashing configuration to the unit
- Insert the SD card into the slot
- Power the unit on, then hold the button labeled AUX for 5 seconds until you hear 5 short beeps and the blue LED turns purple.
- Release the AUX button.
- A successful config flash will result in a *twinkle* sound
<p align="right">(<a href="#readme-top">back to top</a>)</p>
