// include/SignalingClient.h
#ifndef SIGNALING_CLIENT_H
#define SIGNALING_CLIENT_H

#include <libwebsockets.h>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>

#include "SignalingProtocol.h"

class SignalingClient {
public:
    SignalingClient(const std::string& serverUrl);
    ~SignalingClient();

    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;

    // Message handling
    void sendMessage(const SignalingMessage& message);

    // Callback registration for incoming messages
    typedef std::function<void(const SignalingMessage&)> MessageCallback;
    void setMessageCallback(MessageCallback callback);

    // Event loop management
    void startEventLoop();
    void stopEventLoop();

private:
    // Libwebsockets context and connection
    struct lws_context* context;
    struct lws* wsi;
    std::string serverUrl;

    // Connection state
    std::atomic<bool> connected;
    std::atomic<bool> running;

    // Message handling
    std::mutex messageMutex;
    std::queue<SignalingMessage> incomingMessages;
    MessageCallback messageCallback;

    // Internal callback for libwebsockets
    static int callback_signaling(
        struct lws* wsi,
        enum lws_callback_reasons reason,
        void* user,
        void* in,
        size_t len
    );

    // Websocket frame processing
    void processIncomingMessage(const char* data, size_t len);

    // Libwebsockets protocol definition
    static struct lws_protocols protocols[];

    // Event loop thread
    std::thread eventLoopThread;
    void runEventLoop();
};

#endif // SIGNALING_CLIENT_H