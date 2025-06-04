<a id="readme-top"></a>
  <h1 align="center">DL32-S3</h1>
  <p align="center">
    The ESP32 powered smart door lock controller
    <br />
  </p>
<p>**NOTE:** The current version of Adafruit_Neopixel.h is causing multiple models of ESP32 C3, C6, and *S3* (On which the DL32-S3 is based) to throw RMT errors and crash.
The bug is being tracked at <a href="https://github.com/adafruit/Adafruit_NeoPixel/issues/432">here</a><br/>
1.12.4-current are affected. In the interim, please use version 1.12.3<br/></p>

<!-- ABOUT THE PROJECT -->
## About The Project
An ongoing effort to create the perfect smart lock.<br/><br/>
In short, the DL32-S3 is a smart lock controller that can be interacted with via RFID, keypad, MQTT, HTTP, FTP, Serial, and supports a varierty of accessories. It has a fully-featured web interface and supports OTS updates.<br/><br/>
You can download the full user manual <a href="https://github.com/Mark-Roly/dl32-arduino/releases/latest/download/DL32-S3_UserManual.pdf">here</a>.

<!-- GETTING STARTED -->
## Getting Started

Since the controller is capable of running offline, no configuration file is required unless you intend to take advantage of online features such as web interface, remote unlock/monitoring, mqtt, ftp, OTA Updates, etc

### Preparing the SD card with configuration file dl32.json
The easiest way to perform initial configuration, is by transferring your configuration file to the unit using an SD card. Create a text file on a FAT formatted SD card and name the file dl32.json (case sensitive).

Use the below sample dl32.json configuration as a base:
 ```   
// Sample configuration file for DL32-S3 dl32.json
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
    "mqtt_auth": true,
    "mqtt_user": "mqttuser",
    "mqtt_password": "mqttpass",
    "magsr_present" : true,
    "ftp_enabled": false,
    "ftp_user": "ftpuser",
    "ftp_password": "ftppass",
}
```

See descriptions of supported configuration parameters below:

<table width="1050">
<tr><th width="140">Attribute</th><th width="150">Type, default val</th><th width="660">Description</th></tr>
<tr><td width="140">wifi_enabled</td><td width="150">(bool, false)</td><td width="660">Dictates whether or not the onboard mqtt client is enabled</td></tr>
<tr><td width="140">wifi_ssid</td><td width="150">(string)</td><td width="660">SSID for wifi network connection</td></tr>
<tr><td width="140">wifi_password</td><td width="150">(string)</td><td width="660">Password for wifi network connection</td></tr>
<tr><td width="140">mqtt_enabled</td><td width="150">(bool, false)</td><td width="660">Dictates whether or not the onboard mqtt client is enabled</td></tr>
<tr><td width="140">mqtt_server</td><td width="150">(string)</td><td width="660">Hostname or IP address of remote mqtt broker</td></tr>
<tr><td width="140">mqtt_topic</td><td width="150">(string, "dl32s3")</td><td width="660">mqtt topic base to use for sub topics</td></tr>
<tr><td width="140">mqtt_client_name</td><td width="150">(string, "dl32s3")</td><td width="660">Client name to use for mqtt connection</td></tr>
<tr><td width="140">mqtt_tls</td><td width="150">(bool, false)</td><td width="660">Dictates whether connection to the broker uses tls private key authentication (Disregards mqtt_auth, mqtt_user and mqtt_password when true. Requires 3x cert/key files. See doc)</td></tr>
<tr><td width="140">mqtt_auth</td><td width="150">(bool, false)</td><td width="660">Dictates whether username/password credentials should be used for mqtt connection. Not valid when mqtt_tls is enabled.</td></tr>
<tr><td width="140">mqtt_user</td><td width="150">(string, &ldquo;dl32s3&rdquo;)</td><td width="660">Username for mqtt broker connection. Not valid when mqtt_tls is enabled.</td></tr>
<tr><td width="140">mqtt_password</td><td width="150">(string, &ldquo;dl32s3&rdquo;)</td><td width="660">Password for mqtt broker connection. Not valid when mqtt_tls is enabled.</td></tr>
<tr><td width="140">magsr_present</td><td width="150">(bool, false)</td><td width="660">Dictates whether or not a connected magnetic door sensor should be used</td></tr>
<tr><td width="140">ftp_enabled</td><td width="150">(bool, false)</td><td width="660">Dictates whether or not the onboard ftp service is enabled</td></tr>
<tr><td width="140">ftp_user</td><td width="150">(string, &ldquo;dl32s3&rdquo;)</td><td width="660">Username for ftp connections</td></tr>
<tr><td width="140">ftp_password</td><td width="150">(string, &ldquo;dl32s3&rdquo;)</td><td width="660">Password for ftp connections</td></tr>
<tr><td width="140">exitUnlockDur_S</td><td width="150">(bool, 5)</td><td width="660">Time in seconds that the lock will remain unlocked for when exit button is pressed</td></tr>
<tr><td width="140">httpUnlockDur_S</td><td width="150">(bool, 5)</td><td width="660">Time in seconds that the lock will remain unlocked for when Web UI unlock is performed</td></tr>
<tr><td width="140">keyUnlockDur_S</td><td width="150">(bool, 5)</td><td width="660">Time in seconds that the lock will remain unlocked for when valid RFID key is read</td></tr>
<tr><td width="140">cmndUnlockDur_S</td><td width="150">(bool, 5)</td><td width="660">Time in seconds that the lock will remain unlocked for when url/mqtt/serial unlock command is sent</td></tr>
<tr><td width="140">badKeyLockoutDur_S</td><td width="150">(bool, 5)</td><td width="660">Time in seconds that the unit will turn unresponsive for with the red led illuminated when an unauthorized key is read</td></tr>
<tr><td width="140">exitButMinThresh_Ms</td><td width="150">(bool, 50)</td><td width="660">Minimum time in miliseconds that the exit button must be pressed for to register an unlock</td></tr>
<tr><td width="140">keyWaitDur_Ms</td><td width="150">(bool, 1500)</td><td width="660">Timeout in miliseconds between keypad digits eg: max time between button presses for keypad input</td></tr>
<tr><td width="140">garageMomentaryDur_Ms</td><td width="150">(bool, 500)</td><td width="660">Time in miliseconds that the momentary signal will be on when in garage mode</td></tr>
<tr><td width="140">addKeyTimeoutDur_S</td><td width="150">(bool, 10)</td><td width="660">Time in seconds that the unit will await a new card when in Key Add Mode before timing out</td></tr>
<tr><td width="140">buzzer_gh</td><td width="150">(int)</td><td width="660">GPIO Header pin to reassign the buzzer output to.</td></tr>
<tr><td width="140">neopix_gh</td><td width="150">(int)</td><td width="660">GPIO Header pin to reassign neopixel output to. (Requires external LLC)</td></tr>
<tr><td width="140">lockRelay_gh</td><td width="150">(int)</td><td width="660">GPIO Header pin to reassign lock relay output to.</td></tr>
<tr><td width="140">AUXButton_gh</td><td width="150">(int)</td><td width="660">GPIO Header pin to reassign AUX button input to.</td></tr>
<tr><td width="140">exitButton_gh</td><td width="150">(int)</td><td width="660">GPIO Header pin to reassign exit button input to.</td></tr>
<tr><td width="140">bellButton_gh</td><td width="150">(int)</td><td width="660">GPIO Header pin to reassign bell button input to.</td></tr>
<tr><td width="140">magSensor_gh</td><td width="150">(int)</td><td width="660">GPIO Header pin to reassign magnetic door sensor input to.</td></tr>
<tr><td width="140">wiegand_0_gh</td><td width="150">(int)</td><td width="660">GPIO Header pin to reassign wiegand 0 input to. (Requires external LLC)</td></tr>
<tr><td width="140">wiegand_1_gh</td><td width="150">(int)</td><td width="660">GPIO Header pin to reassign wiegand 1 input to. (Requires external LLC)</td></tr>
</table>

### Flashing configuration to the unit
- Use a FAT32-formatted SD card that is 32gb or smaller
- Copy your dl32.json configuration file to the SD card (filename is case senstitive)
- Insert the SD card into the slot
- Power the unit on, then hold the button labeled AUX for 5 seconds until you hear 5 short beeps and the blue LED turns purple.
- Release the AUX button.
- A successful config flash will result in a *twinkle* sound
<p align="right">(<a href="#readme-top">back to top</a>)</p>
