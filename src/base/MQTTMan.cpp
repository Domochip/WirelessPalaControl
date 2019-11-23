#include "MQTTMan.h"

void MQTTMan::prepareTopic(String &topic)
{
    if (topic.indexOf(F("$sn$")) != -1)
    {
        char sn[9];
        sprintf_P(sn, PSTR("%08x"), ESP.getChipId());
        topic.replace(F("$sn$"), sn);
    }

    if (topic.indexOf(F("$mac$")) != -1)
        topic.replace(F("$mac$"), WiFi.macAddress());

    if (topic.indexOf(F("$model$")) != -1)
        topic.replace(F("$model$"), APPLICATION1_NAME);

    //check for final slash
    if (topic.length() && topic.charAt(topic.length() - 1) != '/')
        topic += '/';
}

bool MQTTMan::connect(bool firstConnection)
{
    if (!WiFi.isConnected())
        return false;

    char sn[9];
    sprintf_P(sn, PSTR("%08x"), ESP.getChipId());

    //generate clientID
    String clientID(F(APPLICATION1_NAME));
    clientID += sn;

    //Connect
    char *username = (_username[0] ? _username : nullptr);
    char *password = (_username[0] ? _password : nullptr);
    char *willTopic = (_connectedAndWillTopic[0] ? _connectedAndWillTopic : nullptr);
    const char *willMessage = (_connectedAndWillTopic[0] ? "0" : nullptr);
    PubSubClient::connect(clientID.c_str(), username, password, willTopic, 0, false, willMessage);

    if (connected())
    {
        if (_connectedAndWillTopic[0])
            publish(_connectedAndWillTopic, "1");

        //Subscribe to needed topic
        if (_connectedCallBack)
            _connectedCallBack(this, firstConnection);
    }

    return connected();
}

MQTTMan &MQTTMan::setConnectedAndWillTopic(const char *topic)
{
    if (!topic)
        _connectedAndWillTopic[0] = 0;
    else if (strlen(topic) < sizeof(_connectedAndWillTopic))
        strcpy(_connectedAndWillTopic, topic);

    return *this;
}

MQTTMan &MQTTMan::setConnectedCallback(CONNECTED_CALLBACK_SIGNATURE connectedCallback)
{
    _connectedCallBack = connectedCallback;
    return *this;
}

bool MQTTMan::connect(const char *username, const char *password)
{
    //check logins
    if (username && strlen(username) >= sizeof(_username))
        return false;
    if (password && strlen(password) >= sizeof(_password))
        return false;

    if (username)
        strcpy(_username, username);
    else
        _username[0] = 0;

    if (password)
        strcpy(_password, password);
    else
        _password[0] = 0;

    return connect(true);
}

void MQTTMan::disconnect()
{
    //publish disconnected just before disconnect...
    if (_connectedAndWillTopic[0])
        publish(_connectedAndWillTopic, "0");

    //Stop MQTT Reconnect
    _mqttReconnectTicker.detach();
    //Disconnect
    if (connected()) //Issue #598 : disconnect() crash if client not yet set
        PubSubClient::disconnect();
}

bool MQTTMan::loop()
{
    if (_needMqttReconnect)
    {
        _needMqttReconnect = false;
        LOG_SERIAL.print(F("MQTT Reconnection : "));
        if (connect(false))
            LOG_SERIAL.println(F("OK"));
        else
            LOG_SERIAL.println(F("Failed"));
    }

    //if not connected and reconnect ticker not started
    if (!connected() && !_mqttReconnectTicker.active())
    {
        LOG_SERIAL.println(F("MQTT Disconnected"));
        //set Ticker to reconnect after 20 or 60 sec (Wifi connected or not)
        _mqttReconnectTicker.once_scheduled((WiFi.isConnected() ? 20 : 60), [this]() { _needMqttReconnect = true; _mqttReconnectTicker.detach(); });
    }

    return PubSubClient::loop();
}