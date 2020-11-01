#ifndef MQTTMan_h
#define MQTTMan_h

#include "..\Main.h"
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <Ticker.h>
#include <PubSubClient.h>

#define CONNECTED_CALLBACK_SIGNATURE std::function<void(MQTTMan * mqttMan, bool firstConnection)>

class MQTTMan : private PubSubClient
{
private:
    char _username[64] = {0};
    char _password[64] = {0};
    char _connectedAndWillTopic[96] = {0};
    bool _needMqttReconnect = false;
    Ticker _mqttReconnectTicker;

    CONNECTED_CALLBACK_SIGNATURE _connectedCallBack = nullptr;

    bool connect(bool firstConnection);

public:
    static void prepareTopic(String &topic);

    using PubSubClient::setClient;
    using PubSubClient::setServer;
    MQTTMan &setConnectedAndWillTopic(const char *topic);
    MQTTMan &setConnectedCallback(CONNECTED_CALLBACK_SIGNATURE connectedCallback);
    using PubSubClient::setCallback;
    bool connect(const char *username = nullptr, const char *password = nullptr);
    using PubSubClient::connected;
    void disconnect();
    using PubSubClient::beginPublish;
    using PubSubClient::endPublish;
    using PubSubClient::publish;
    using PubSubClient::publish_P;
    using PubSubClient::state;
    using PubSubClient::subscribe;
    bool loop();
};

#endif