/*
    esp32HttpOTA
    esp32 http firmware/SPIFFS OTA 
    Date: Mars 2020
    Author: Franck RONDOT
    www.franck-rondot.com
    Based on esp32FOTA by Chris Joyce <https://github.com/chrisjoyce911/esp32FOTA/esp32FOTA>
    I changed somes codes and add SPIFFS update support
    Purpose: Perform an OTA update of firmware or SPIFFS from a bin located on a webserver (HTTP Only)

    If your use Google Cloud Platform for data storage of json and .bin change the datatype to : application/octet-stream
    If you have some trouble to connect deactivate the WAF on your sub domain, some WAF detect some user agent, cookies...

    You could try CURL to verify your connection ie : curl --head https://update.website.com/appname/check.php
*/

#ifndef esp32HttpJsonOTA_h
#define esp32HttpJsonOTA_h

#include <Arduino.h>

#include <esp_log.h>

static const char* OTATAG = "OTA"; // ESP_LOG TAG

struct otaConfig {
  char host[50]; // hostname or ip of firmware serveur without HTTP and subdir just the hostname like update.server.com
  char bin[60]; // the path of bin file start with / like /ver1/firmware.bin
  char type[9]; // the type of firmware FIRMWARE or SPIFFS
  int port = 80; // the port of the server
};

class esp32HttpJsonOTA
{
public:
  esp32HttpJsonOTA(String firmwareName, String firmwareType, int firmwareVersion, String urlJson);

  void    forceUpdate(String firmwareHost, int firmwarePort, String firmwarePath, String firmwareType);
  void    execOTA();
  bool    execHTTPcheck();
  bool    useDeviceID;
  void    setVer(int);

private:
  String    getHeaderValue(String header, String headerName); // Return header value
  String    getDeviceID(); // Device ID
  String    _firmwareName = ""; // Name of firmware
  int       _firmwareVersion = 0; // Version of firmware
  String    _checkURL = ""; // URL of json
  otaConfig _cnf; // struct otaConfig
};

#endif
