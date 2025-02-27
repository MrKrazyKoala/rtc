// src/SignalingClient.cpp
#include "SignalingClient.h"
#include <thread>
#include <chrono>
#include <stdexcept>

SignalingClient::SignalingClient(const std::string& url)
    : serverUrl(url),
    context(nullptr),
    wsi(nullptr),
    connected(false),
    running(false),
    messageCallback(nullptr) {

    // Initialize libwebsockets context
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.iface = NULL;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    context = lws_create_context(&info);
    if (!context) {
        throw std::runtime_error("Failed to create libwebsockets context");
    }
}

SignalingClient::~SignalingClient() {
    disconnect();

    if (context) {
        lws_context_destroy(context);
        context = nullptr;
    }
}

bool SignalingClient::connect() {
    if (connected) {
        return true;
    }

    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));

    ccinfo.context = context;
    ccinfo.port = 8080;
    ccinfo.address = serverUrl.c_str();
    ccinfo.path = "/";
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = "signaling";
    ccinfo.pwsi = &wsi;

    // Connect to the server
    struct lws* connection = lws_client_connect_via_info(&ccinfo);

    if (!connection) {
        return false;
    }

    connected = true;
    return true;
}

void SignalingClient::disconnect() {
    if (!connected) {
        return;
    }

    stopEventLoop();

    // Close the websocket connection
    if (wsi) {
        lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
        wsi = nullptr;
    }

    connected = false;
}

bool SignalingClient::isConnected() const {
    return connected;
}

void SignalingClient::sendMessage(const SignalingMessage& message) {
    if (!connected) {
        throw std::runtime_error("Not connected to signaling server");
    }

    std::string serializedMsg = message.serialize();

    // Allocate buffer with lws protocol requirements
    size_t bufLen = LWS_PRE + serializedMsg.length();
    unsigned char* buf = new unsigned char[bufLen];

    // Copy message to buffer after LWS_PRE
    memcpy(buf + LWS_PRE, serializedMsg.c_str(), serializedMsg.length());

    // Send via libwebsockets
    int n = lws_write(wsi, buf + LWS_PRE, serializedMsg.length(), LWS_WRITE_TEXT);

    delete[] buf;

    if (n < serializedMsg.length()) {
        throw std::runtime_error("Failed to send complete message");
    }
}

void SignalingClient::setMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(messageMutex);
    messageCallback = callback;
}

void SignalingClient::startEventLoop() {
    // Continued from previous implementation
    if (running) {
        return;
    }

    running = true;
    eventLoopThread = std::thread(&SignalingClient::runEventLoop, this);
}

void SignalingClient::stopEventLoop() {
    running = false;
    if (eventLoopThread.joinable()) {
        eventLoopThread.join();
    }
}

void SignalingClient::runEventLoop() {
    while (running && connected) {
        // Service any pending libwebsockets events
        lws_service(context, 50);

        // Small sleep to prevent tight looping
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void SignalingClient::processIncomingMessage(const char* data, size_t len) {
    try {
        // Deserialize the message
        SignalingMessage message = SignalingMessage::deserialize(
            std::string(data, len)
        );

        // If a callback is set, invoke it
        std::lock_guard<std::mutex> lock(messageMutex);
        if (messageCallback) {
            messageCallback(message);
        }
        else {
            // Alternatively, store in a queue if no callback is set
            incomingMessages.push(message);
        }
    }
    catch (const std::exception& e) {
        // Log deserialization errors
        // In a real implementation, use proper logging
        fprintf(stderr, "Message deserialization error: %s\n", e.what());
    }
}

// Libwebsockets protocol callback
int SignalingClient::callback_signaling(
    struct lws* wsi,
    enum lws_callback_reasons reason,
    void* user,
    void* in,
    size_t len
) {
    SignalingClient* client = static_cast<SignalingClient*>(
        lws_context_user(lws_get_context(wsi))
        );

    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        printf("WebSocket connection established\n");
        client->connected = true;
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE: {
        // Process incoming WebSocket message
        client->processIncomingMessage(
            static_cast<const char*>(in),
            len
        );
        break;
    }

    case LWS_CALLBACK_CLIENT_CLOSED:
        printf("WebSocket connection closed\n");
        client->connected = false;
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        printf("WebSocket connection error\n");
        client->connected = false;
        break;

    default:
        break;
    }

    return 0;
}

// Static protocol definition
struct lws_protocols SignalingClient::protocols[] = {
    {
        "signaling",          // Protocol name
        SignalingClient::callback_signaling,  // Callback function
        0,                    // Per-session data size
        0,                    // Maximum frame size
    },
    { NULL, NULL, 0, 0 }     // Terminating null protocol
};