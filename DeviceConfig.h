// include/DeviceConfig.h
#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <string>
#include <cjson/cJSON.h>
#include <fstream>
#include <cstdlib>
#include <stdexcept>

class DeviceConfig {
public:
    std::string deviceId;
    std::string macAddress;
    std::string cloudSerialNumber;
    std::string rtspUrl;
    uint16_t defaultRtpPort;

    // Load configuration from a JSON file
    static DeviceConfig loadFromFile(const std::string& configPath) {
        DeviceConfig config;

        FILE* file = fopen(configPath.c_str(), "r");
        if (!file) {
            throw std::runtime_error("Could not open config file");
        }

        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        char* buffer = new char[fileSize + 1];
        fread(buffer, 1, fileSize, file);
        buffer[fileSize] = '\0';
        fclose(file);

        cJSON* configJson = cJSON_Parse(buffer);
        delete[] buffer;

        if (!configJson) {
            throw std::runtime_error("Failed to parse JSON");
        }

        // Parsing example
        cJSON* deviceId = cJSON_GetObjectItemCaseSensitive(configJson, "device_id");
        config.deviceId = deviceId && deviceId->valuestring ?
            deviceId->valuestring : generateDeviceId();

        // Continue parsing other fields similarly

        cJSON_Delete(configJson);
        return config;
    }

private:
    // Generate a unique device ID if not provided
    static std::string generateDeviceId() {
        // Generate a semi-unique ID based on MAC or random number
        char buffer[64];
        FILE* pipe = popen("cat /sys/class/net/*/address | head -n 1", "r");
        if (pipe) {
            if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                // Remove newline
                buffer[strcspn(buffer, "\n")] = 0;
                pclose(pipe);
                return std::string(buffer);
            }
            pclose(pipe);
        }

        // Fallback to random number if MAC retrieval fails
        snprintf(buffer, sizeof(buffer), "device-%d", rand());
        return std::string(buffer);
    }
};

#endif // DEVICE_CONFIG_H