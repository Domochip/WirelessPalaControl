#include "MQTTMan.h"

void MQTTMan::prepareTopic(String &topic)
{

    //check for final slash
    if (topic.length() && topic.charAt(topic.length() - 1) != '/')
        topic += '/';

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
    if (!m_username[0])
        PubSubClient::connect(clientID.c_str());
    else
        PubSubClient::connect(clientID.c_str(), m_username, m_password);

    if (connected())
    {
        //Subscribe to needed topic
        if (m_connectedCallBack)
            m_connectedCallBack(this, firstConnection);
    }

    return connected();
}

MQTTMan &MQTTMan::setConnectedCallback(CONNECTED_CALLBACK_SIGNATURE connectedCallback)
{
    m_connectedCallBack = connectedCallback;
    return *this;
}

bool MQTTMan::connect(char *username, char *password)
{
    //check logins
    if (username && strlen(username) >= sizeof(m_username))
        return false;
    if (password && strlen(password) >= sizeof(m_password))
        return false;

    if (username)
        strcpy(m_username, username);
    else
        m_username[0] = 0;

    if (password)
        strcpy(m_password, password);
    else
        m_password[0] = 0;

    return connect(true);
}

void MQTTMan::disconnect()
{
    //Stop MQTT Reconnect
    m_mqttReconnectTicker.detach();
    //Disconnect
    if (connected()) //Issue #598 : disconnect() crash if client not yet set
        PubSubClient::disconnect();
}

bool MQTTMan::loop()
{
    if (m_needMqttReconnect)
    {
        m_needMqttReconnect = false;
        LOG_SERIAL.print(F("MQTT Reconnection : "));
        if (connect(false))
            LOG_SERIAL.println(F("OK"));
        else
            LOG_SERIAL.println(F("Failed"));
    }

    //if not connected and reconnect ticker not started
    if (!connected() && !m_mqttReconnectTicker.active())
    {
        LOG_SERIAL.println(F("MQTT Disconnected"));
        //set Ticker to reconnect after 20 or 60 sec (Wifi connected or not)
        m_mqttReconnectTicker.once_scheduled((WiFi.isConnected() ? 20 : 60), [this]() { m_needMqttReconnect = true; m_mqttReconnectTicker.detach(); });
    }

    return PubSubClient::loop();
}