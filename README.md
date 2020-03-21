# esp32HttpJsonOTA
ESP32 HTTP OTA Firmware and SPIFFS update with Json config and version control  
Perform an OTA update of firmware or SPIFFS from a bin located on a webserver (HTTP Only)  
without server side script. Use simple webserver, Google Cloud Platform storage, and and others  
simple HTTP file share system.  
Date: Mars 2020  
Author: Franck RONDOT https://www.franck-rondot.com  
Based on esp32FOTA by Chris Joyce <https://github.com/chrisjoyce911/esp32FOTA/esp32FOTA>  
I changed somes codes and add SPIFFS update support  

## Nota : 
If your use Google Cloud Platform for data storage of json and .bin change the datatype to : application/octet-stream  
If you have some trouble to connect deactivate the WAF on your sub domain, some WAF detect some user agent, cookies...  

You could try CURL to verify your connection ie : curl --head https://update.website.com/appname/check.php

## Example
### json file for sketch firmware
```json
{
	"name": "ESP32APPFR",
	"type": "FIRMWARE",
	"version": 1,
	"host": "storage.googleapis.com",
	"port": 80,
	"bin": "/update/esp32ehjo/fw-esp32ehjo.bin"
}
```
### json file for SPIFFS
```json
{
    "name": "ESP32APPFR",
    "type": "SPIFFS",
    "version": 1,
    "host": "storage.googleapis.com",
    "port": 80,
    "bin": "/update/esp32ehjo/fs-esp32ehjo.bin"
}
```
### Cpp example
```cpp
  #include <Arduino.h>
  #include <WiFi.h>
  #include <esp_log.h>
  #include <SPIFFS.h>

  #include "esp32HttpJsonOTA.h"

  // Nom et version pour la mise a jour du FW
  #define SSID "WIFISSID"
  #define PASSWORD "WIFIPASSWORD"
  #define OTA_NAME "ESP32APPFR"
  #define OTA_VER 1
  #define FWOTA_JSONURL "http://update.website.com/update/esp32ehjo/fw-esp32ehjo.json"
  #define FSOTA_JSONURL "http://update.website.com/update/esp32ehjo/fs-esp32ehjo.json"

  esp32HttpJsonOTA majFW(OTA_NAME, "FIRMWARE", OTA_VER, FWOTA_JSONURL);
  esp32HttpJsonOTA majFS(OTA_NAME, "SPIFFS", OTA_VER, FSOTA_JSONURL);

  static const char* TAG = "EHJO";

  int verFS()
  {
  File myFile;
  String ver = "";

    // Init SPIFFS
    if(!SPIFFS.begin(true))
    {
        ESP_LOGE(TAG, "Mount error of SPIFFS...");
        return 0;
    }
    else
    {
      myFile = SPIFFS.open("/ver", "r");
      
      if(myFile)
      {
          while(myFile.available())
          {
              ver = myFile.readString();
          }

          myFile.close();
          ESP_LOGD(TAG, "SPIFFS version : %d", ver.toInt());

          return ver.toInt();
      }
      else
      {
          return 1;
      }
    }
  }

  bool newVerFW()
  {
      bool maj = majFW.execHTTPcheck();
      ESP_LOGD(TAG, "Sketch update available : %s", maj ? "Yes" : "No");
      return maj;
  }

  void updateFW()
  {
      majFW.execOTA();
  }

  bool newVerFS()
  {
      majFS.setVer(verFS());
      bool maj = majFS.execHTTPcheck();
      ESP_LOGD(TAG, "SPIFFS update available : %s", maj ? "Yes" : "No");
      return maj;
  }

  void updateFS()
  {
      majFS.execOTA();
  }

  void setup_wifi()
  {
    WiFi.begin(SSID, PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(".");
      delay(500);
    }

    ESP_LOGI(TAG, "IP address : %s", WiFi.localIP().toString().c_str());
  }

  void forceUpd()
  {
    majFW.forceUpdate("192.168.0.100", 80, "/firmware.bin", "FIRMWARE");
  }

  void checkDeviceID()
  {
    majFW.useDeviceID = true;
    bool updatedNeeded = majFW.execHTTPcheck();
    if (updatedNeeded)
    {
      majFW.execOTA();
    }
  }

  void setup()
  {
    // esp32 log lavel
    esp_log_level_set("*", ESP_LOG_VERBOSE);

    Serial.begin(115200);
    
    setup_wifi();

    if (newVerFW())
    {
      ESP_LOGI(TAG, "Sketch update available !");
      updateFW();
    }

    if (newVerFS())
    {
      ESP_LOGI(TAG, "SPIFFS update available !");
      updateFS();
    }
  }

  void loop()
  {
    delay(100);
  }
```
