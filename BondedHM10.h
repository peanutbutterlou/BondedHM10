
/*
 * BondedHM10 is an Arduino Library that facilitates the provisioning, connection and 
 * communication between two HM-10 Bluetooth LE 4.0 modules.
 * (https://github.com/peanutbutterlou/BondedHM10)
 * 
 * Writen by Peanutbutterlou. 
 */

#ifndef BondedHM10_h
#define BondedHM10_h

#include "Arduino.h"
#include "Print.h"
#include "WString.h"

//#define DEBUG 1
//#define VERBOSE 1


class BondedHM10: public Print
{
public:

    static const uint16_t DEFAULT_MAX_BYTES_TO_READ = 256;

    enum Role
    {
        Peripheral = 0,
        Central = 1
    };


    enum BaudRate
    {
        Baud_9600 = 0,
        Baud_19200 = 1,
        Baud_38400 = 2,
        Baud_57600 = 3,
        Baud_115200 = 4,
        Baud_4800 = 5,
        Baud_2400 = 6,
        Baud_1200 = 7,
        Baud_230400 = 8,
        Baud_Default = Baud_9600
    };


    enum WhitelistSlot
    {
        Slot1 = 1,
        Slot2 = 2,
        Slot3 = 3,
        DefaultSlot = Slot1
    };


    enum BondMode
    {
        NoAuth = 0,
        AuthNoPin = 1,
        AuthWithPin = 2,
        AuthAndBond = 3
    };


    enum WorkType
    {
        AutoStart = 0,
        ManualStart = 1
    };


    BondedHM10(const Role role, const char* remoteAddress, const byte statePin, const byte resetPin);

    bool provision(const BaudRate baudRate);

    bool begin(Stream& stream, bool autoConnect = true);
    bool ready();
    void loop(uint16_t maxBytesToRead = DEFAULT_MAX_BYTES_TO_READ);

    void setConsoleModeEnabled(const bool enabled);
    bool getConsoleModeEnabled();

    byte getConnectedOutputPin();
    void setConnectedOutputPin(const byte connectedOutputPin);
    void clearConnectedOutputPin();

    byte getDataTransmittedOutputPin();
    void setDataTransmittedOutputPin(const byte dataTransmittedOutputPin);
    void clearDataTransmittedOutputPin();

    typedef void (*ConnectedDelegate)(bool reconnected);
    void setConnectedHandler(ConnectedDelegate connectedHandler);

    typedef void (*DisconnectedDelegate)(void);
    void setDisconnectedHandler(DisconnectedDelegate disconnectedHandler);

    byte getDisconnectReconnectInputPin();
    void setDisconnectReconnectInputPin(const byte disconnectReconnectInputPin);
    void clearDisconnectReconnectInputPin();

    bool getAutoReconnectEnabled();
    void setAutoReconnectEnabled(const bool enabled);

    long getAutoReconnectTimeout();
    void setAutoReconnectTimeout(const unsigned long timeout);

    bool isConnected(bool testForTransientConnection);
    bool isConnected();

    bool disconnect();
    bool connectToPeripheral();
    bool reconnectToPeripheral();

    void reset();

    bool getDeviceName(char* deviceName);
    bool setDeviceName(const char* deviceName);

    bool getAddress(char* address);
    bool getFirmwareVersion(char* versionStr);
    bool getRole(Role& role);
    bool getBaudRate(BaudRate& baudRate);
    bool getWorkType(WorkType& workType);
    bool getLastConnectedAddress(char* address);
    bool getWhitelistEnabled(bool& enabled);
    bool getWhitelistAddress(const WhitelistSlot slot, char* address);
    bool getBondMode(BondMode& bondMode);

#ifdef DEBUG
    void printDeviceConfig();
#endif


    bool writeEvent(uint16_t id, const uint8_t* content, const uint16_t length);
    bool writeEvent(uint16_t id, const char* content);
    bool writeEvent(uint16_t id, const char* content, const uint16_t length);
    bool writeEvent(uint16_t id, const __FlashStringHelper* content);

    typedef void (*EventReceivedUInt8Delegate)(const uint16_t id, const uint8_t* content, const uint16_t length);
    void setEventReceivedHandler(EventReceivedUInt8Delegate eventReceivedHandler);

    typedef void (*EventReceivedCharDelegate)(const uint16_t id, const char* content, const uint16_t length);
    void setEventReceivedHandler(EventReceivedCharDelegate eventReceivedHandler);


    bool writeMessage(const uint8_t* content, const uint16_t length);
    bool writeMessage(const char* content);
    bool writeMessage(const char* content, const uint16_t length);
    bool writeMessage(const __FlashStringHelper* content);

    typedef void (*MessageReceivedUInt8Delegate)(const uint8_t* content, const uint16_t length);
    void setMessageReceivedHandler(MessageReceivedUInt8Delegate messageReceivedHandler);

    typedef void (*MessageReceivedCharDelegate)(const char* content, const uint16_t length);
    void setMessageReceivedHandler(MessageReceivedCharDelegate messageReceivedHandler);

    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t* buffer, size_t size);
    virtual int availableForWrite();

    static void handleTransmissionTimerInterrupt();


private:

    bool provision_Central();
    bool provision_Peripheral();

    bool begin_Central(bool autoConnect);
    bool begin_Peripheral(bool autoConnect);

    void sendCommand_Internal(const char* command, const bool query, const char* param);

    bool sendCommandWithExpectedResponse(const char* command, const bool query, const char* param, const char* expectedResponse, char* actualResponse, const uint16_t timeout);
    bool sendCommandWithExpectedResponse(const char* command, const bool query, const char* param, const char* expectedResponse, char* actualResponse);

    bool waitForResponse_Internal(const char* expectedResponse, char* actualResponse, const uint16_t timeout, const unsigned long startTime);
    bool waitForResponse(const char* expectedResponse, char* actualResponse, const uint16_t timeout);

    void parseCommandResponseValue(const char* responsePrefix, const char* response, char* value);

    void clearString(char* str, const uint16_t startIndex);
    void clearString(char* str);

    void onConnect();
    void onDisconnect();

    void detectAndHandleConnection();
    void detectAndHandleDisconnectReconnect();

    uint16_t getFlashStringHelperLength(const __FlashStringHelper* content);
    void writeFlashStringHelperContent(const __FlashStringHelper* content);

    void resetPrefixDetection();
    void resetEventParsing();
    void resetContentParsing();

    void startTransmissionTimer();
    void stopTransmissionTimer();

    bool startWork();
    bool setConnectedOutputStatePins(const char* pinHex);
    bool setRole(const Role role);
    bool setBaudRate(const BaudRate baudRate);
    bool setWorkType(const WorkType workType);
    bool clearLastConnectedAddress();
    bool setWhitelistEnabled(const bool enabled);
    bool setWhitelistAddress(const WhitelistSlot slot, const char* address);
    bool setBondMode(const BondMode bondMode);


    static BondedHM10* _instance;

    char* _responseStr = NULL;
    char* _commandStr = NULL;
    char* _lastConnectedAddressStr = NULL;
    uint8_t* _contentBuffer = NULL;

    Role _role;
    char* _remoteAddress = NULL;
    int8_t _statePin = -1;
    int8_t _resetPin = -1;
    int8_t _connectedOutputPin = -1;
    int8_t _dataTransmittedOutputPin = -1;
    volatile uint16_t _transmissionTimer = 0;
    volatile long _transmissionTimerStoppedTimestamp = 0;
    int8_t _disconnectReconnectInputPin = -1;
    bool _autoReconnectEnabled = false;
    long _autoReconnectTimeout = 30000;
    bool _consoleModeEnabled = false;
    Stream* _stream;
    BaudRate _baudRate = BaudRate::Baud_Default;
    bool _initialized = false;
    bool _connected = false;
    bool _connecting = false;
    bool _disconnected = false;
    bool _manuallyDisconnected = false;
    long _lastConnectAttemptTimestamp = 0;
    uint8_t _prefixCursor = 0;
    uint16_t _contentCursor = 0;
    bool _messageSuspected = false;
    bool _messageDetected = false;
    bool _eventSuspected = false;
    bool _eventDetected = false;
    byte _eventIDLowByte = 0;
    byte _eventIDHighByte = 0;
    uint16_t _eventID = 0;
    byte _lengthHighByte = 0;
    byte _lengthLowByte = 0;
    uint16_t _contentLength = 0;

    ConnectedDelegate _connectedHandler = NULL;
    DisconnectedDelegate _disconnectedHandler = NULL;
    EventReceivedUInt8Delegate _eventReceivedUInt8Handler = NULL;
    EventReceivedCharDelegate _eventReceivedCharHandler = NULL;
    MessageReceivedUInt8Delegate _messageReceivedUInt8Handler = NULL;
    MessageReceivedCharDelegate _messageReceivedCharHandler = NULL;


};

#endif