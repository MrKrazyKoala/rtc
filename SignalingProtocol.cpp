// src/SignalingProtocol.cpp
#include "SignalingProtocol.h"
#include <stdexcept>
#include <cstring>

// Type Conversion Functions
std::string signalingMessageTypeToString(SignalingMessageType type) {
    switch (type) {
    case SignalingMessageType::REGISTER: return "REGISTER";
    case SignalingMessageType::REQUEST: return "REQUEST";
    case SignalingMessageType::RESPONSE: return "RESPONSE";
    case SignalingMessageType::OFFER: return "OFFER";
    case SignalingMessageType::ANSWER: return "ANSWER";
    case SignalingMessageType::ICE: return "ICE";
    case SignalingMessageType::HEARTBEAT: return "HEARTBEAT";
    case SignalingMessageType::ERROR: return "ERROR";
    case SignalingMessageType::DISCONNECT: return "DISCONNECT";
    case SignalingMessageType::STATUS: return "STATUS";
    case SignalingMessageType::CONFIG_UPDATE: return "CONFIG_UPDATE";
    case SignalingMessageType::STREAM_INFO: return "STREAM_INFO";
    case SignalingMessageType::LOG: return "LOG";
    case SignalingMessageType::DIAGNOSTICS: return "DIAGNOSTICS";
    default: return "UNKNOWN";
    }
}

SignalingMessageType stringToSignalingMessageType(const std::string& typeStr) {
    if (typeStr == "REGISTER") return SignalingMessageType::REGISTER;
    if (typeStr == "REQUEST") return SignalingMessageType::REQUEST;
    if (typeStr == "RESPONSE") return SignalingMessageType::RESPONSE;
    if (typeStr == "OFFER") return SignalingMessageType::OFFER;
    if (typeStr == "ANSWER") return SignalingMessageType::ANSWER;
    if (typeStr == "ICE") return SignalingMessageType::ICE;
    if (typeStr == "HEARTBEAT") return SignalingMessageType::HEARTBEAT;
    if (typeStr == "ERROR") return SignalingMessageType::ERROR;
    if (typeStr == "DISCONNECT") return SignalingMessageType::DISCONNECT;
    if (typeStr == "STATUS") return SignalingMessageType::STATUS;
    if (typeStr == "CONFIG_UPDATE") return SignalingMessageType::CONFIG_UPDATE;
    if (typeStr == "STREAM_INFO") return SignalingMessageType::STREAM_INFO;
    if (typeStr == "LOG") return SignalingMessageType::LOG;
    if (typeStr == "DIAGNOSTICS") return SignalingMessageType::DIAGNOSTICS;

    throw std::invalid_argument("Unknown message type: " + typeStr);
}

// Constructors and Destructor
SignalingMessage::SignalingMessage()
    : type(SignalingMessageType::REGISTER), payload(nullptr) {
}

SignalingMessage::SignalingMessage(SignalingMessageType msgType, const std::string& msgId)
    : type(msgType), id(msgId), payload(nullptr) {
}

SignalingMessage::~SignalingMessage() {
    // Free the payload if it exists
    if (payload) {
        cJSON_Delete(payload);
    }
}

// Copy Constructor
SignalingMessage::SignalingMessage(const SignalingMessage& other)
    : type(other.type), id(other.id), metadata(other.metadata) {
    // Deep copy payload
    payload = other.payload ? cJSON_Duplicate(other.payload, 1) : nullptr;
}

// Copy Assignment Operator
SignalingMessage& SignalingMessage::operator=(const SignalingMessage& other) {
    if (this != &other) {
        // Free existing payload
        if (payload) {
            cJSON_Delete(payload);
        }

        // Copy members
        type = other.type;
        id = other.id;
        metadata = other.metadata;

        // Deep copy payload
        payload = other.payload ? cJSON_Duplicate(other.payload, 1) : nullptr;
    }
    return *this;
}

// Move Constructor
SignalingMessage::SignalingMessage(SignalingMessage&& other) noexcept
    : type(other.type), id(std::move(other.id)),
    payload(other.payload), metadata(std::move(other.metadata)) {
    // Nullify the other object's payload to prevent double-free
    other.payload = nullptr;
}

// Move Assignment Operator
SignalingMessage& SignalingMessage::operator=(SignalingMessage&& other) noexcept {
    if (this != &other) {
        // Free existing payload
        if (payload) {
            cJSON_Delete(payload);
        }

        // Move members
        type = other.type;
        id = std::move(other.id);
        payload = other.payload;
        metadata = std::move(other.metadata);

        // Nullify the other object's payload
        other.payload = nullptr;
    }
    return *this;
}

// Serialization Method
std::string SignalingMessage::serialize() const {
    // Create root JSON object
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        throw std::runtime_error("Failed to create JSON object");
    }

    try {
        // Add message type
        cJSON_AddStringToObject(root, "type",
            signalingMessageTypeToString(type).c_str());

        // Add ID
        cJSON_AddStringToObject(root, "id", id.c_str());

        // Add payload if exists
        if (payload) {
            cJSON_AddItemToObject(root, "payload",
                cJSON_Duplicate(payload, 1));
        }

        // Add metadata
        if (!metadata.empty()) {
            cJSON* metadataJson = cJSON_CreateObject();
            for (const auto& pair : metadata) {
                cJSON_AddStringToObject(metadataJson,
                    pair.first.c_str(), pair.second.c_str());
            }
            cJSON_AddItemToObject(root, "metadata", metadataJson);
        }

        // Convert to string
        char* jsonStr = cJSON_Print(root);
        if (!jsonStr) {
            throw std::runtime_error("Failed to convert JSON to string");
        }

        std::string result(jsonStr);
        free(jsonStr);  // cJSON uses malloc, so use free
        cJSON_Delete(root);

        return result;
    }
    catch (...) {
        // Ensure root is deleted in case of any exception
        cJSON_Delete(root);
        throw;
    }
}

// Deserialization Method
SignalingMessage SignalingMessage::deserialize(const std::string& jsonStr) {
    // Parse JSON string
    cJSON* root = cJSON_Parse(jsonStr.c_str());
    if (!root) {
        const char* error = cJSON_GetErrorPtr();
        throw std::runtime_error(error ? error : "Failed to parse JSON");
    }

    try {
        // Create message object
        SignalingMessage msg;

        // Extract type (required)
        cJSON* typeJson = cJSON_GetObjectItemCaseSensitive(root, "type");
        if (!cJSON_IsString(typeJson)) {
            throw std::invalid_argument("Missing or invalid message type");
        }
        msg.setType(stringToSignalingMessageType(typeJson->valuestring));

        // Extract ID (required)
        cJSON* idJson = cJSON_GetObjectItemCaseSensitive(root, "id");
        if (!cJSON_IsString(idJson)) {
            throw std::invalid_argument("Missing or invalid message ID");
        }
        msg.setId(idJson->valuestring);

        // Extract payload (optional)
        cJSON* payloadJson = cJSON_GetObjectItemCaseSensitive(root, "payload");
        if (payloadJson) {
            msg.setPayload(cJSON_Duplicate(payloadJson, 1));
        }

        // Extract metadata (optional)
        cJSON* metadataJson = cJSON_GetObjectItemCaseSensitive(root, "metadata");
        if (cJSON_IsObject(metadataJson)) {
            cJSON* metaItem;
            cJSON_ArrayForEach(metaItem, metadataJson) {
                if (cJSON_IsString(metaItem)) {
                    msg.addMetadata(metaItem->string, metaItem->valuestring);
                }
            }
        }

        // Clean up
        cJSON_Delete(root);

        return msg;
    }
    catch (...) {
        // Ensure root is deleted in case of any exception
        cJSON_Delete(root);
        throw;
    }
}

// Setters
void SignalingMessage::setType(SignalingMessageType newType) {
    type = newType;
}

void SignalingMessage::setId(const std::string& newId) {
    id = newId;
}

void SignalingMessage::setPayload(cJSON* newPayload) {
    // Free existing payload
    if (payload) {
        cJSON_Delete(payload);
    }

    // Set new payload (create a duplicate to manage memory)
    payload = newPayload ? cJSON_Duplicate(newPayload, 1) : nullptr;
}

void SignalingMessage::addMetadata(const std::string& key, const std::string& value) {
    metadata[key] = value;
}

// Getters
SignalingMessageType SignalingMessage::getType() const {
    return type;
}

std::string SignalingMessage::getId() const {
    return id;
}

cJSON* SignalingMessage::getPayload() const {
    // Return a duplicate to prevent external modification of internal payload
    return payload ? cJSON_Duplicate(payload, 1) : nullptr;
}

std::string SignalingMessage::getMetadata(const std::string& key) const {
    auto it = metadata.find(key);
    return (it != metadata.end()) ? it->second : "";
}

// Validation Method
bool SignalingMessage::validate() const {
    // Basic validation: ID must not be empty
    if (id.empty()) return false;

    // Type-specific validations
    switch (type) {
    case SignalingMessageType::REGISTER:
        // Require payload for registration
        return payload != nullptr;

    case SignalingMessageType::REQUEST:
        // Require payload for request
        return payload != nullptr;

    case SignalingMessageType::OFFER:
    case SignalingMessageType::ANSWER: {
        // Require payload with SDP
        if (!payload) return false;

        cJSON* sdp = cJSON_GetObjectItemCaseSensitive(payload, "sdp");
        return sdp && cJSON_IsString(sdp);
    }

    case SignalingMessageType::ICE: {
        // Require payload with candidate
        if (!payload) return false;

        cJSON* candidate = cJSON_GetObjectItemCaseSensitive(payload, "candidate");
        return candidate && cJSON_IsString(candidate);
    }

    case SignalingMessageType::HEARTBEAT:
        // Heartbeat can be empty
        return true;

    case SignalingMessageType::ERROR:
        // Error should have a message
        return payload != nullptr;

    default:
        // For other types, just ensure basic requirements are met
        return true;
    }
}