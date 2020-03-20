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

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "esp32HttpJsonOTA.h"

esp32HttpJsonOTA::esp32HttpJsonOTA(String firmwareName, String firmwareType, int firmwareVersion, String urlJson)
{
    _firmwareName = firmwareName;
    strlcpy(_cnf.type, firmwareType.c_str(), sizeof(_cnf.type));
    _firmwareVersion = firmwareVersion;
    _checkURL = urlJson;
    
    useDeviceID = false;
}

// Set the version after initialisation
void esp32HttpJsonOTA::setVer(int ver)
{
    _firmwareVersion = ver;
}

// Utility to extract header value from headers
String esp32HttpJsonOTA::getHeaderValue(String header, String headerName)
{
    return header.substring(strlen(headerName.c_str()));
}

// OTA updating of firmware or SPIFFS
void esp32HttpJsonOTA::execOTA()
{
WiFiClient client;
int contentLength = 0;
bool isValidContentType = false;
String header = "";
String get = "";

    ESP_LOGI(OTATAG, "Connecting to: %s port %d", _cnf.host, _cnf.port);
    // Connect to Webserver
    if (client.connect(_cnf.host, _cnf.port))
    {
        // Fecthing the bin
        ESP_LOGI(OTATAG, "Fetching Bin: %s", _cnf.bin);

        // Get the contents of the bin file
        get = String("GET ") + String(_cnf.bin) + String(" HTTP/1.1\r\nHost: ") + String(_cnf.host) + String("\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n");
        ESP_LOGD(OTATAG, "GET : %s", get.c_str());
        client.print(get.c_str());

        unsigned long timeout = millis();
        while (client.available() == 0)
        {
            if (millis() - timeout > 3000)
            {
                ESP_LOGI(OTATAG, "Client Timeout !");
                client.stop(); // Free resources
                return;
            }
        }

        while (client.available())
        {
            // Read line till /n
            String line = client.readStringUntil('\n');
            // Remove space, to check if the line is end of headers
            line.trim();

            if (!line.length())
            {
                // Headers ended and get the OTA started
                break; 
            }

            // Check if the HTTP Response is 200 else break and Exit Update
            if (line.startsWith("HTTP/1.1"))
            {
                if (line.indexOf("200") < 0)
                {
                    ESP_LOGD(OTATAG, "Got a non 200 status code from server. Exiting OTA Update.");
                    break;
                }
            }

            // Extract headers

            // Content length
            header = line.substring(0, 16);

            if (header.equalsIgnoreCase("Content-Length: "))
            {
                contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
                ESP_LOGI(OTATAG, "Got %d bytes from server", contentLength);
            }

            // Content type
            header = line.substring(0, 14);
            if (header.equalsIgnoreCase("Content-Type: "))
            {
                String contentType = getHeaderValue(line, header);
                ESP_LOGI(OTATAG, "Got %s payload.", contentType.c_str());
                if (contentType == "application/octet-stream")
                {
                    isValidContentType = true;
                }
            }
        }
    }
    else
    {
        // Connect to webserver failed
        ESP_LOGI(OTATAG, "Connection to %s failed. Please check your setup", _cnf.host);
        client.stop(); // Free resources
        return;
    }

    // Check what is the contentLength and if content type is `application/octet-stream`
    ESP_LOGD(OTATAG, "contentLength : %s , isValidContentType : %s", String(contentLength),String(isValidContentType));

    // Check contentLength and content type
    if (contentLength && isValidContentType)
    {
        // Check if there is enough to OTA Update and set the type of update FIRMWARE or SPIFFS
        int cmd = (String(_cnf.type) == "SPIFFS") ? U_SPIFFS : U_FLASH;
        ESP_LOGI(OTATAG, "OTA type : %s", (cmd == U_SPIFFS) ? "SPIFFS" : "FIRMWARE");
        bool canBegin = Update.begin(contentLength, cmd);

        // If yes, begin
        if (canBegin)
        {
            ESP_LOGI("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
            // No activity would appear on the Serial monitor
            // So be patient. This may take 2 - 5mins to complete
            size_t written = Update.writeStream(client);

            if (written == contentLength)
            {
                ESP_LOGI(OTATAG, "Written : %s successfully", String(written));
            }
            else
            {
                ESP_LOGE(OTATAG, "Written only : %s/%s retry !", String(written), String(contentLength));
            }

            if (Update.end())
            {
                ESP_LOGI(OTATAG, "OTA done!");
                if (Update.isFinished())
                {
                    ESP_LOGI(OTATAG, "Update successfully completed. Rebooting.");
                    ESP.restart();
                }
                else
                {
                    ESP_LOGE(OTATAG, "Update not finished? Something went wrong!");
                }
            }
            else
            {
                ESP_LOGE(OTATAG, "Error Occurred. Error #: %s", String(Update.getError()));
            }
        }
        else
        {
            // Not enough space to begin OTA
            ESP_LOGE(OTATAG, "Not enough space to begin OTA");
        }
    }
    else
    {
        ESP_LOGE(OTATAG, "There was no content in the response");
    }

    client.stop(); // Free resources
}

bool esp32HttpJsonOTA::execHTTPcheck()
{

    String useURL;

    if (useDeviceID)
    {
        // String deviceID = getDeviceID() ;
        useURL = _checkURL + "?id=" + getDeviceID();
    }
    else
    {
        useURL = _checkURL;
    }

    _cnf.port = 80;

    ESP_LOGD(OTATAG, "Getting HTTP : %s", useURL.c_str());

    if ((WiFi.status() == WL_CONNECTED))
    { 
        //Check the current connection status

        HTTPClient http;

        http.begin(useURL);        // Specify the URL
        int httpCode = http.GET(); // Make the request
        
        // Check if the file is receveid
        if (httpCode == 200)
        {
            const size_t capacity = JSON_OBJECT_SIZE(6) + 200;
            DynamicJsonDocument JSONDocument(capacity);

            DeserializationError err = deserializeJson(JSONDocument, http.getStream());

            if (err)
            { 
                //Check for errors in parsing
                ESP_LOGD(OTATAG, "Parsing failed");
                http.end(); //Free the resources
                return false;
            }

            const char* pName = JSONDocument["name"];
            String jsName(pName);

            int jsVer = JSONDocument["version"];

            strlcpy(_cnf.type, JSONDocument["type"], sizeof(_cnf.type));
            strlcpy(_cnf.host, JSONDocument["host"], sizeof(_cnf.host));
            strlcpy(_cnf.bin, JSONDocument["bin"], sizeof(_cnf.bin));
            _cnf.port = JSONDocument["port"];

            ESP_LOGD(OTATAG, "name : %s", jsName.c_str());
            ESP_LOGD(OTATAG, "type : %s", _cnf.type);
            ESP_LOGD(OTATAG, "version : %d", jsVer);
            ESP_LOGD(OTATAG, "host : %s", _cnf.host);
            ESP_LOGD(OTATAG, "port : %d", _cnf.port);
            ESP_LOGD(OTATAG, "bin : %s", _cnf.bin);

            if (jsVer > _firmwareVersion && jsName == _firmwareName)
            {
                http.end(); //Free the resources
                return true; // Update available
            }
            else
            {
                http.end(); //Free the resources
                return false; // Unavailable update
            }
        }
        else
        {
            ESP_LOGE(OTATAG, "Error on HTTP request");
            http.end(); //Free the resources
            return false;
        }

        http.end(); //Free the resources
        return false;
    }
    return false;
}

String esp32HttpJsonOTA::getDeviceID()
{
    char deviceid[21];
    uint64_t chipid;
    chipid = ESP.getEfuseMac();
    sprintf(deviceid, "%" PRIu64, chipid);
    String thisID(deviceid);
    return thisID;
}

// Force a firmware update regartless on current version
void esp32HttpJsonOTA::forceUpdate(String firmwareHost, int firmwarePort, String firmwarePath, String firmwareType)
{
    strlcpy(_cnf.host, firmwareHost.c_str(), sizeof(_cnf.host));
    strlcpy(_cnf.bin, firmwarePath.c_str(), sizeof(_cnf.bin));
    strlcpy(_cnf.type, firmwareType.c_str(), sizeof(_cnf.type));

    _cnf.port = firmwarePort;

    execOTA();
}
