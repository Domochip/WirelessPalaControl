#ifndef MQTTMan_h
#define MQTTMan_h

#include "..\Main.h"
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <PubSubClient.h>

#define CONNECTED_CALLBACK_SIGNATURE std::function<void(MQTTMan * mqttMan, bool firstConnection)>

class MQTTMan : private PubSubClient
{
private:
    char m_username[64] = {0};
    char m_password[64] = {0};
    char m_connectedAndWillTopic[96] = {0};
    bool m_needMqttReconnect = false;
    Ticker m_mqttReconnectTicker;

    CONNECTED_CALLBACK_SIGNATURE m_connectedCallBack = nullptr;

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