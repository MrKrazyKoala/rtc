// src/main.cpp
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <memory>

#include "SignalingClient.h"
#include "DeviceConfig.h"

// Global signal handling
namespace {
    std::atomic<bool> g_running(true);
}

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    g_running = false;
}

// Device configuration and initialization
class DeviceManager {
private:
    DeviceConfig config;
    std::unique_ptr<SignalingClient> signalingClient;

public:
    DeviceManager(const std::string& configPath, const std::string& signalingUrl)
        : config(DeviceConfig::loadFromFile(configPath)),
        signalingClient(new SignalingClient(signalingUrl)) {

        // Setup message callback
        signalingClient->setMessageCallback(
            std::bind(&DeviceManager::handleSignalingMessage, this, std::placeholders::_1)
        );
    }

    bool initialize() {
        // Connect to signaling server
        if (!signalingClient->connect()) {
            std::cerr << "Failed to connect to signaling server\n";
            return false;
        }

        // Start event loop
        signalingClient->startEventLoop();

        // Send initial registration
        sendRegistration();

        return true;
    }

    void run() {
        while (g_running) {
            // Periodic tasks
            sendHeartbeat();

            // Small sleep to prevent tight looping
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }

        // Cleanup
        signalingClient->disconnect();
    }

private:
    void sendRegistration() {
        SignalingMessage registrationMsg(SignalingMessageType::REGISTER, config.deviceId);

        // Prepare registration payload
        cJSON* payload = cJSON_CreateObject();
        cJSON_AddStringToObject(payload, "device_type", "camera");
        cJSON_AddStringToObject(payload, "mac_address", config.macAddress.c_str());
        cJSON_AddStringToObject(payload, "csn", config.cloudSerialNumber.c_str());

        registrationMsg.setPayload(payload);

        // Add metadata
        registrationMsg.addMetadata("version", "1.0.0");
        registrationMsg.addMetadata("stream_count", "1");

        // Send registration
        signalingClient->sendMessage(registrationMsg);

        // Free the JSON object
        cJSON_Delete(payload);
    }

    void sendHeartbeat() {
        SignalingMessage heartbeat(SignalingMessageType::HEARTBEAT, config.deviceId);

        // Optional payload with system status
        cJSON* payload = cJSON_CreateObject();
        cJSON_AddNumberToObject(payload, "uptime", getSystemUptime());
        cJSON_AddNumberToObject(payload, "temperature", getSystemTemperature());

        heartbeat.setPayload(payload);

        signalingClient->sendMessage(heartbeat);

        // Free the JSON object
        cJSON_Delete(payload);
    }

    void handleSignalingMessage(const SignalingMessage& msg) {
        switch (msg.getType()) {
        case SignalingMessageType::REQUEST:
            handleStreamRequest(msg);
            break;
        case SignalingMessageType::OFFER:
            handleWebRTCOffer(msg);
            break;
        default:
            std::cout << "Received unhandled message type: "
                << signalingMessageTypeToString(msg.getType()) << std::endl;
        }
    }

    void handleStreamRequest(const SignalingMessage& msg) {
        // Logic to handle stream request
        SignalingMessage response(SignalingMessageType::RESPONSE, config.deviceId);
        response.addMetadata("request_id", msg.getId());

        cJSON* payload = cJSON_CreateObject();
        cJSON_AddStringToObject(payload, "status", "available");
        cJSON_AddStringToObject(payload, "stream_url", config.rtspUrl.c_str());

        response.setPayload(payload);

        signalingClient->sendMessage(response);

        // Free the JSON object
        cJSON_Delete(payload);
    }

    void handleWebRTCOffer(const SignalingMessage& msg) {
        // Placeholder for WebRTC offer processing
        // This would typically involve creating an answer and ICE candidates
        std::cout << "Received WebRTC offer" << std::endl;
    }

    // System information helpers (mock implementations)
    double getSystemUptime() {
        // TODO: Implement actual uptime retrieval
        FILE* file = fopen("/proc/uptime", "r");
        if (file) {
            double uptime;
            if (fscanf(file, "%lf", &uptime) == 1) {
                fclose(file);
                return uptime;
            }
            fclose(file);
        }
        return 3600.0; // 1 hour fallback
    }

    double getSystemTemperature() {
        // TODO: Implement actual temperature retrieval
        FILE* pipe = popen("cat /sys/class/thermal/thermal_zone0/temp", "r");
        if (pipe) {
            double temp;
            if (fscanf(pipe, "%lf", &temp) == 1) {
                pclose(pipe);
                return temp / 1000.0; // Convert millidegrees to degrees
            }
            pclose(pipe);
        }
        return 45.5; // 45.5°C fallback
    }
};

int main() {
    // Register signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        // Create device manager
        DeviceManager deviceManager(
            "/etc/kinnode/config.json",  // Config path
            "ws://192.30.240.10:8080"    // Signaling server URL
        );

        // Initialize device
        if (!deviceManager.initialize()) {
            std::cerr << "Device initialization failed\n";
            return 1;
        }

        // Run main device loop
        deviceManager.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}