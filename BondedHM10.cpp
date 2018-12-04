#include "BondedHM10.h"

const char *COMMAND_PREFIX = "AT+";
const char *COMMAND_AT = "AT";
const char *QUERY = "?";
const char *AT_RESPONSE = "OK";
const char *COMMAND_NAME = "NAME";
const char *NAME_RESPONSE = "OK+NAME:";
const char *COMMAND_CLEAR = "CLEAR";
const char *CLEAR_RESPONSE = "OK+CLEAR";
const char *AT_LOST_RESPONSE = "OK+LOST";
const char *COMMAND_VERSION = "VERR";
const char *COMMAND_ADDRESS = "ADDR";
const char *ADDRESS_RESPONSE = "OK+ADDR:";
const char *COMMAND_RADD = "RADD";
const char *RADD_RESPONSE = "OK+RADD:";
const char *COMMAND_ROLE = "ROLE";
const char *COMMAND_CON_OUTPUTSTATE_PIN = "AFTC";
const char *COMMAND_START = "START";
const char *COMMAND_CONNECT = "CON";
const char *CONNECT_RESPONSE = "OK+CONN";
const char *CONNECT_FAILURE_RESPONSE = "OK+CONNF";
const char *START_RESPONSE = "OK+START";
const char *COMMAND_WORKTYPE = "IMME";
const char *COMMAND_BAUDRATE = "BAUD";
const char *COMMAND_WHITELISTENABLED = "ALLO";
const char *COMMAND_WHITELIST = "AD";
const char *WHITELIST_RESPONSE = "OK+AD";
const char *COMMAND_BONDMODE = "TYPE";
const char *RESPONSE_OKSET = "OK+Set:";
const char *RESPONSE_OKGET = "OK+Get:";
const char *EMPTY_ADDRESS = "000000000000";
const char *CON_OUTPUTSTATE_PINS = "200";
const long DEFAULT_COMMAND_TIMEOUT = 250;
const long CONNECT_COMMAND_TIMEOUT = 1000;
const long CONNECTING_COMMAND_TIMEOUT = 10250;
const uint8_t TRANSIENTCONNECTTEST_RETRYCOUNT = 3;
const long TRANSIENTCONNECTTEST_DELAY = 500;
const long DISCONNECTRECONNECT_DEBOUNCE_TIMEOUT = 500;
const byte GENERIC_START_BYTE = (byte)'~';
const char *EVENT_PREFIX = "~EVT";
static const size_t PREFIX_LEN = strlen(EVENT_PREFIX); // EVENT_PREFIX must be the same length as MESSAGE_PREFIX.
const char *MESSAGE_PREFIX = "~MSG";
const uint16_t MAX_BYTES_READ_PER_LOOP = 25;
const uint16_t MAX_CONTENT_BUFFER_SIZE = 256;
const uint16_t TRANSMISSION_TIMER_DURATION = 50;         // milliseconds
const uint16_t TRANSMISSION_TIMER_DEBOUNCE_TIMEOUT = 50; // milliseconds

BondedHM10 *BondedHM10::_instance = NULL;

BondedHM10::BondedHM10(const Role role, const char *remoteAddress, const byte statePin, const byte resetPin)
{
  _responseStr = (char *)calloc(32, sizeof(char));
  _commandStr = (char *)calloc(32, sizeof(char));
  _lastConnectedAddressStr = (char *)calloc(13, sizeof(char));
  _contentBuffer = (uint8_t *)calloc(MAX_CONTENT_BUFFER_SIZE + 1, sizeof(uint8_t));

  _role = role;
  _remoteAddress = (char *)remoteAddress;
  _statePin = statePin;
  _resetPin = resetPin;

  _instance = this;
}

bool BondedHM10::provision(const BaudRate baudRate)
{
  setConnectedOutputStatePins(CON_OUTPUTSTATE_PINS);

  BaudRate currentBaudRate;
  if (getBaudRate(currentBaudRate))
  {
    if (currentBaudRate != baudRate)
    {
      setBaudRate(baudRate);
    }
  }
  else
  {
    return false;
  }

  bool success = true;

  if (_role == Role::Central)
  {
    success = provision_Central();
  }
  else
  {
    success = provision_Peripheral();
  }

  reset();

  return success;
}

bool BondedHM10::provision_Central()
{
  bool success = true;

  if (!setRole(Role::Central))
  {
    success = false;
  }

  if (!setWorkType(WorkType::ManualStart))
  {
    success = false;
  }

  if (!setWhitelistEnabled(false))
  {
    success = false;
  }

  if (!setBondMode(BondMode::NoAuth))
  {
    success = false;
  }

  return success;
}

bool BondedHM10::provision_Peripheral()
{
  bool success = true;

  if (!setRole(Role::Peripheral))
  {
    success = false;
  }

  if (!setWorkType(WorkType::AutoStart))
  {
    success = false;
  }

  if (!setWhitelistEnabled(true))
  {
    success = false;
  }

  if (!setWhitelistAddress(WhitelistSlot::Slot1, _remoteAddress))
  {
    success = false;
  }

  if (!setBondMode(BondMode::AuthAndBond))
  {
    success = false;
  }

  return success;
}

bool BondedHM10::begin(Stream &serial, bool autoConnect)
{
  bool success = true;

#ifdef DEBUG
  Serial.print(F("Initializing bluetooth device "));

  if (_role == Role::Central)
  {
    Serial.println(F("(central)..."));
  }
  else
  {
    Serial.println(F("(peripheral)..."));
  }

  Serial.flush();
#endif

  _stream = &serial;

  pinMode(_statePin, INPUT);

  pinMode(_resetPin, OUTPUT);
  digitalWrite(_resetPin, HIGH);
  delay(250); // Need to wait a little bit after setting the reset pin HIGH.

  if (_role == Role::Central)
  {
    success = begin_Central(autoConnect);
  }
  else
  {
    success = begin_Peripheral(autoConnect);
  }

#ifdef DEBUG
  if (success)
  {
    Serial.println(F("Bluetooth device initialized."));
  }
  else
  {
    Serial.println(F("Bluetooth device initialization FAILED."));
  }
#endif

  _initialized = success;

  return success;
}

bool BondedHM10::begin_Central(bool autoConnect)
{
  bool success = false;

  // In order to ACCURATELY detect if there is an active connection at "initialization time", we need to account for the posibility
  // of the central device being in a transient "connected" state. This can happen if the central device is shutdown (i.e. the host
  // device is turned off or loses power, etc.) without having first disconnected from its peripherals, and while a connection to
  // the peripheral device(s) is unavailable the central is powered back on. If the central device is set to start work immediately
  // when powered on (via the AT+IMME1 command), it will automatically attempt to reconnect to the last connected peripheral (that is,
  // if the central is configured to save it's last connected address via the AT+SAVE command) This creates a situation where the
  // central device may have already successfully reconnected to the peripheral before the "initBluetooth" function can is executed.
  // Once the central device has connected to a peripheral, the central's module parameters can only be configured via AT commands
  // transmitted over UART from the peripheral (and ONLY if the "remote control" work mode is enabled on the central device, via the
  // AT+MODE2 command). Any attempt to execute AT commands on the central device while it is connected to a peripheral will result in
  // those commands being transmitted as text to the peripheral over UART. This also means the AT+CLEAR command, and any other AT
  // command that might tell the central device to disconnect or to disable the reconnection attempts, will simply NOT work. The best
  // way to work around the transient "connected" state is to test for an active connection multiple times with a short delay between
  // each test. If all of the tests yield a positive "connected" state, then we can be fairly certain the central device is actively
  // connected to a peripheral.

  if (isConnected(true))
  {
    Serial.println(F("Previous active connection detected. Disconnecting..."));

    bool disconnectSuccessful = disconnect();

#ifdef DEBUG
    if (!disconnectSuccessful)
    {
      Serial.println(F("Disconnect FAILED."));
    }
#endif

    if (!disconnectSuccessful)
    {
      return false;
    }
  }

  success = sendCommandWithExpectedResponse("", false, NULL, AT_RESPONSE, _responseStr); // AT (test)

  if (success)
  {
    if (getLastConnectedAddress(_lastConnectedAddressStr))
    {
      if (strcmp(_lastConnectedAddressStr, _remoteAddress) != 0 && strcmp(_lastConnectedAddressStr, EMPTY_ADDRESS) != 0)
      {
#ifdef DEBUG
        Serial.println(F("Clearing the last connected peripheral address since it does NOT match the hardcoded peripheral address."));
#endif

        success = clearLastConnectedAddress();
      }
    }
    else
    {
      success = false;
    }
  }

  if (success)
  {
    if (autoConnect)
    {
      reconnectToPeripheral();
      _lastConnectAttemptTimestamp = millis();
    }
  }

  return success;
}

bool BondedHM10::begin_Peripheral(bool autoConnect)
{
  bool success = true;

  if (success)
  {
    if (autoConnect)
    {
      _lastConnectAttemptTimestamp = millis();
    }
  }

  return success;
}

bool BondedHM10::ready()
{
  return _initialized;
}

void BondedHM10::resetPrefixDetection()
{
  _prefixCursor = 0;
  _messageSuspected = false;
  _eventSuspected = false;
}

void BondedHM10::resetContentParsing()
{
  _lengthLowByte = 0;
  _lengthHighByte = 0;
  _contentCursor = 0;
  _contentLength = 0;
  clearString((char *)_contentBuffer, 0);
  _messageDetected = false;
  _eventDetected = false;
}

void BondedHM10::resetEventParsing()
{
  _eventIDLowByte = 0;
  _eventIDHighByte = 0;
  _eventID = 0;
  resetContentParsing();
}

void BondedHM10::loop(uint16_t maxBytesToRead)
{
  if (_initialized)
  {
    detectAndHandleConnection();

    if (_role == Role::Central)
    {
      detectAndHandleDisconnectReconnect();
    }

    if (_connected)
    {
      if (_consoleModeEnabled)
      {
        if (Serial)
        {
          // TODO: Add while loops here to do the reads/writes in larger batches.
          // This will also help with support for flashing an LED on communication.

          if (_stream->available())
          {
            if (_dataTransmittedOutputPin >= 0)
            {
              startTransmissionTimer();
            }

            Serial.write(_stream->read());
          }

          if (Serial.available())
          {
            if (_dataTransmittedOutputPin >= 0)
            {
              startTransmissionTimer();
            }

            _stream->write(Serial.read());
          }
        }
      }
      else
      {
        int bytesAvailable = 0;
        uint16_t bytesRead = 0;

        while ((bytesAvailable = _stream->available()) > 0 && bytesRead < MAX_BYTES_READ_PER_LOOP)
        {
          const byte currentByte = (byte)_stream->read();
          bytesRead++;

          if (_dataTransmittedOutputPin >= 0)
          {
            startTransmissionTimer();
          }

#ifdef DEBUG
#ifdef VERBOSE
          Serial.println((char)currentByte);
#endif
#endif

          if (currentByte == GENERIC_START_BYTE)
          {
#ifdef DEBUG
#ifdef VERBOSE
            Serial.println(F("Prefix suspected."));
#endif
#endif

            _prefixCursor = 1; // Move the prefix cursor to the next position.
            resetContentParsing();
          }
          else if (_prefixCursor > 0)
          {
            if (currentByte == MESSAGE_PREFIX[_prefixCursor])
            {
              if (_prefixCursor == 1 || _messageSuspected)
              {
#ifdef DEBUG
#ifdef VERBOSE
                if (!_messageSuspected)
                {
                  Serial.println(F("Message prefix suspected."));
                }
#endif
#endif

                _messageSuspected = true;
                _prefixCursor++;
              }
              else
              {
                resetPrefixDetection();
              }
            }
            else if (currentByte == EVENT_PREFIX[_prefixCursor])
            {
              if (_prefixCursor == 1 || _eventSuspected)
              {
#ifdef DEBUG
#ifdef VERBOSE
                if (!_eventSuspected)
                {
                  Serial.println(F("Event prefix suspected."));
                }
#endif
#endif

                _eventSuspected = true;
                _prefixCursor++;
              }
              else
              {
                resetPrefixDetection();
              }
            }

            // Determine if we've read the full event/message prefix.
            if (_prefixCursor == PREFIX_LEN)
            {
              if (_messageSuspected)
              {
#ifdef DEBUG
#ifdef VERBOSE
                Serial.println(F("Message detected."));
#endif
#endif

                _messageDetected = true;
              }
              else if (_eventSuspected)
              {
#ifdef DEBUG
#ifdef VERBOSE
                Serial.println(F("Event detected."));
#endif
#endif

                _eventDetected = true;
              }

              resetPrefixDetection();
            }
          }
          else if (_eventDetected)
          {
            // The first 2 bytes of the event form a uint16_t that reveals the Event ID.
            if (_contentCursor == 0)
            {
              _eventIDLowByte = currentByte;
            }
            else if (_contentCursor == 1)
            {
              _eventIDHighByte = currentByte;
              _eventID = (uint16_t)word(_eventIDHighByte, _eventIDLowByte);

#ifdef DEBUG
#ifdef VERBOSE
              Serial.print(F("Event ID detected: "));
              Serial.println(_eventID);
#endif
#endif
            }
            else if (_contentCursor == 2)
            {
              _lengthLowByte = currentByte;
            }
            else if (_contentCursor == 3)
            {
              _lengthHighByte = currentByte;
              _contentLength = (uint16_t)word(_lengthHighByte, _lengthLowByte);
              clearString((char *)_contentBuffer);

              if (_contentLength < 1)
              {
#ifdef DEBUG
#ifdef VERBOSE
                Serial.print(F("Invalid event content length detected: "));
                Serial.println(_contentLength);
#endif
#endif

                resetEventParsing();
                continue;
              }

#ifdef DEBUG
#ifdef VERBOSE
              Serial.print(F("Length detected: "));
              Serial.println(_contentLength);
#endif
#endif
            }
            else if (_contentLength > 0 && _contentCursor >= 4 && (_contentCursor < (_contentLength + 4))) // content length + 2 bytes for the event ID + 2 bytes for the length
            {
              _contentBuffer[_contentCursor - 4] = (uint8_t)currentByte;
            }
            else
            {
              resetEventParsing();
              continue;
            }

            _contentCursor++;

            if (_contentCursor == (_contentLength + 4)) // _contentLength + 2 bytes for the eventID + 2 bytes for the length.
            {
              clearString((char *)_contentBuffer, _contentLength);

#ifdef DEBUG
#ifdef VERBOSE
              Serial.print(F("Event received:"));
              Serial.println((char *)_contentBuffer);
#endif
#endif

              if (_eventReceivedUInt8Handler)
              {
                _eventReceivedUInt8Handler(_eventID, _contentBuffer, _contentLength);
              }

              if (_eventReceivedCharHandler)
              {
                _eventReceivedCharHandler(_eventID, (char *)_contentBuffer, _contentLength);
              }

              resetEventParsing();
            }
          }
          else if (_messageDetected)
          {
            // The first 2 bytes of the message form a uint16_t that reveals the length of the message content.
            if (_contentCursor == 0)
            {
              _lengthLowByte = currentByte;
            }
            else if (_contentCursor == 1)
            {
              _lengthHighByte = currentByte;
              _contentLength = (uint16_t)word(_lengthHighByte, _lengthLowByte);
              clearString((char *)_contentBuffer);

              if (_contentLength < 1)
              {
#ifdef DEBUG
#ifdef VERBOSE
                Serial.print(F("Invalid message content length detected: "));
                Serial.println(_contentLength);
#endif
#endif

                resetContentParsing();
                continue;
              }

#ifdef DEBUG
#ifdef VERBOSE
              Serial.print(F("Length detected: "));
              Serial.println(_contentLength);
#endif
#endif
            }
            else if (_contentCursor < (_contentLength + 2)) // content length + 2 bytes for the length
            {
              _contentBuffer[_contentCursor - 2] = (uint8_t)currentByte;
            }
            else
            {
              resetContentParsing();
              continue;
            }

            _contentCursor++;

            if (_contentCursor == (_contentLength + 2))
            {
              clearString((char *)_contentBuffer, _contentLength);

#ifdef DEBUG
#ifdef VERBOSE
              Serial.print(F("Message received:"));
              Serial.println((char *)_contentBuffer);
#endif
#endif

              if (_messageReceivedUInt8Handler)
              {
                _messageReceivedUInt8Handler(_contentBuffer, _contentLength);
              }

              if (_messageReceivedCharHandler)
              {
                _messageReceivedCharHandler((char *)_contentBuffer, _contentLength);
              }

              resetContentParsing();
            }
          }
        }
      }
    }
  }
}

void BondedHM10::setConsoleModeEnabled(const bool consoleModeEnabled)
{
  _consoleModeEnabled = consoleModeEnabled;
}

bool BondedHM10::getConsoleModeEnabled()
{
  return _consoleModeEnabled;
}

byte BondedHM10::getConnectedOutputPin()
{
  return _connectedOutputPin;
}

void BondedHM10::setConnectedOutputPin(const byte connectedOutputPin)
{
  if (connectedOutputPin < 0)
  {
    clearConnectedOutputPin();
  }
  else
  {
    _connectedOutputPin = connectedOutputPin;
  }
}

void BondedHM10::clearConnectedOutputPin()
{
  digitalWrite(_connectedOutputPin, LOW);
  _connectedOutputPin = -1;
}

byte BondedHM10::getDataTransmittedOutputPin()
{
  return _dataTransmittedOutputPin;
}

void BondedHM10::setDataTransmittedOutputPin(const byte dataTransmittedOutputPin)
{
  if (dataTransmittedOutputPin < 0)
  {
    clearDataTransmittedOutputPin();
  }
  else
  {
    _dataTransmittedOutputPin = dataTransmittedOutputPin;
  }
}

void BondedHM10::clearDataTransmittedOutputPin()
{
  digitalWrite(_dataTransmittedOutputPin, LOW);
  _dataTransmittedOutputPin = -1;
  stopTransmissionTimer();
}

void BondedHM10::setConnectedHandler(ConnectedDelegate connectedHandler)
{
  _connectedHandler = connectedHandler;
}

void BondedHM10::setDisconnectedHandler(DisconnectedDelegate disconnectedHandler)
{
  _disconnectedHandler = disconnectedHandler;
}

byte BondedHM10::getDisconnectReconnectInputPin()
{
  return _disconnectReconnectInputPin;
}

void BondedHM10::setDisconnectReconnectInputPin(const byte disconnectReconnectInputPin)
{
  _disconnectReconnectInputPin = disconnectReconnectInputPin;
}

void BondedHM10::clearDisconnectReconnectInputPin()
{
  _disconnectReconnectInputPin = -1;
}

bool BondedHM10::getAutoReconnectEnabled()
{
  return _autoReconnectEnabled;
}

void BondedHM10::setAutoReconnectEnabled(const bool enabled)
{
  _autoReconnectEnabled = enabled;
}

long BondedHM10::getAutoReconnectTimeout()
{
  return _autoReconnectTimeout;
}

void BondedHM10::setAutoReconnectTimeout(const unsigned long timeout)
{
  _autoReconnectTimeout = timeout;
}

bool BondedHM10::isConnected(bool testForTransientConnection)
{
  if (!testForTransientConnection)
  {
    return (digitalRead(_statePin) == HIGH);
  }
  else
  {
    byte testIndex = 0;

    while (testIndex < TRANSIENTCONNECTTEST_RETRYCOUNT)
    {
      if (digitalRead(_statePin) == LOW)
      {
#ifdef DEBUG
#ifdef VERBOSE
        Serial.println(F("STATE pin on chip reads LOW (connection NOT active)."));
#endif
#endif

        return false;
      }
      else
      {
#ifdef DEBUG
#ifdef VERBOSE
        Serial.println(F("STATE pin on chip reads HIGH (connection active)."));
#endif
#endif
      }

      delay(TRANSIENTCONNECTTEST_DELAY);
      testIndex++;
    }

    return true;
  }
}

bool BondedHM10::isConnected()
{
  return isConnected(false);
}

bool BondedHM10::disconnect()
{
  if (_initialized && (_role == Role::Central) && _connected)
  {
    reset();
    detectAndHandleConnection();

    bool success = false;

    if (!isConnected(true))
    {
      success = true;
    }

#ifdef DEBUG
    if (success)
    {
      Serial.println(F("Disconnected."));
    }
    else
    {
      Serial.println(F("Disconnect FAILED."));
    }

    Serial.flush();
#endif

    return success;
  }
  else
  {
    return false;
  }
}

bool BondedHM10::startWork()
{
  bool success = sendCommandWithExpectedResponse(COMMAND_START, false, NULL, START_RESPONSE, _responseStr);

#ifdef DEBUG
  Serial.print(F("START WORK"));

  if (!success)
  {
    Serial.print(F(": FAILURE: "));
    Serial.println(_responseStr);
  }
  else
  {
    Serial.println();
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::connectToPeripheral()
{
  if (_role != Role::Central)
  {
    return false;
  }

  if (_connecting)
  {
#ifdef DEBUG
    Serial.println(F("connectToPeripheral exited early. Connecting in progress..."));
#endif

    return false;
  }

#ifdef DEBUG
  Serial.print(F("Attempting to connect to peripheral at "));
  Serial.print(_remoteAddress);
  Serial.println(F("..."));
#endif

  _connecting = true;
  bool success = sendCommandWithExpectedResponse(COMMAND_CONNECT, false, _remoteAddress, CONNECT_RESPONSE, _responseStr, CONNECT_COMMAND_TIMEOUT);

  if (success)
  {
    char *responseCode = (char *)calloc(2, sizeof(char));

    parseCommandResponseValue(CONNECT_RESPONSE, _responseStr, responseCode);

    if (responseCode[0] == 'A')
    {
      if (waitForResponse(CONNECT_FAILURE_RESPONSE, _responseStr, CONNECTING_COMMAND_TIMEOUT))
      {
        // We received at AT+CONNF response (failure).
        if (isConnected())
        {
          success = true;

#ifdef DEBUG
          Serial.println(F("SUCCESS: Already connected."));
#endif
        }
        else
        {
          success = false;

#ifdef DEBUG
          Serial.println(F("FAILURE: Peripheral unavailable."));
#endif
        }
      }
      else
      {
        if (isConnected())
        {
          success = true;

#ifdef DEBUG
          Serial.println(F("SUCCESS: Connected."));
#endif
        }
        else
        {
          success = false;

#ifdef DEBUG
          Serial.println(F("FAILURE: Peripheral unavailable."));
#endif
        }
      }
    }
    else if (responseCode[0] == 'E')
    {
      success = false;

#ifdef DEBUG
      Serial.println(F("FAILURE: Connect Error."));
#endif
    }
    else if (responseCode[0] == 'F')
    {
      success = false;

#ifdef DEBUG
      Serial.println(F("FAILURE: Peripheral unavailable."));
#endif
    }
    else if (strlen(responseCode) == 0)
    {
      success = true;

#ifdef DEBUG
      Serial.println(F("SUCCESS: Already connected."));
#endif
    }
    else
    {
      success = false;

#ifdef DEBUG
      Serial.print(F("FAILURE: Unknown response code: "));
      Serial.println(responseCode[0]);
#endif
    }

    free(responseCode);
  }
  else
  {
#ifdef DEBUG
    Serial.println(F("FAILURE: Reason unknown."));
#endif
  }

  _connecting = false;

  return success;
}

bool BondedHM10::reconnectToPeripheral()
{
  if (_connecting)
  {
#ifdef DEBUG
    Serial.println(F("reconnectToPeripheral exited early. Connecting in progress..."));
#endif

    return false;
  }

  bool success = startWork();

  if (success)
  {
    // Get the last connected address. If we think it's a valid address, then just call startWork to auto connect to the
    // peripheral. If it is not a valid address, then attempt to manually connect to the hardcoded peripheral address.
    char *lastConnectedAddress = (char *)calloc(13, sizeof(char)); // We aren't using btLastConnectedAddressStr here to avoid having to sync access to the variable.

    if (getLastConnectedAddress(lastConnectedAddress))
    {
      if (strcmp(lastConnectedAddress, EMPTY_ADDRESS) == 0)
      {
        success = connectToPeripheral();
      }
      else
      {
        _connecting = true;
      }
    }

    free(lastConnectedAddress);
  }

  return success;
}

void BondedHM10::reset()
{
  digitalWrite(_resetPin, LOW);
  delay(101); // Hold the RESET pin LOW for at least 100 miliseconds to trigger a reset of the bluetooth device.
  digitalWrite(_resetPin, HIGH);

  delay(250);
}

bool BondedHM10::setConnectedOutputStatePins(const char *pinHex)
{
  if (strlen(pinHex) != 3)
  {
#ifdef DEBUG
    Serial.println(F("setConnectionOutputStatePins failed. Only a 3 digit hex string is accepted as input."));
#endif

    return false;
  }

  bool success = sendCommandWithExpectedResponse(COMMAND_CON_OUTPUTSTATE_PIN, false, pinHex, RESPONSE_OKSET, _responseStr, 2000);

#ifdef DEBUG
  Serial.print(F("SET Connected Output State Pins: "));

  if (success)
  {
    Serial.println(pinHex);
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::getDeviceName(char *deviceName)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_NAME, true, NULL, NAME_RESPONSE, _responseStr);

  if (success)
  {
    parseCommandResponseValue(NAME_RESPONSE, _responseStr, deviceName);
  }

#ifdef DEBUG
  Serial.print(F("Device Name: "));

  if (success)
  {
    Serial.println(deviceName);
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  if (!success)
  {
    clearString(deviceName);
  }

  return success;
}

bool BondedHM10::setDeviceName(const char *deviceName)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_NAME, false, deviceName, RESPONSE_OKSET, _responseStr);

#ifdef DEBUG
  Serial.print(F("SET Device name: "));
  Serial.println(deviceName);

  if (!success)
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::getAddress(char *address)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_ADDRESS, true, NULL, ADDRESS_RESPONSE, _responseStr);

  if (success)
  {
    parseCommandResponseValue(ADDRESS_RESPONSE, _responseStr, address);
  }

#ifdef DEBUG
  Serial.print(F("Address: "));

  if (success)
  {
    Serial.println(address);
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  if (!success)
  {
    clearString(address);
  }

  return success;
}

bool BondedHM10::getFirmwareVersion(char *versionStr)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_VERSION, true, NULL, NULL, _responseStr);

  if (success)
  {
    parseCommandResponseValue(NULL, _responseStr, versionStr);
  }

#ifdef DEBUG
  Serial.print(F("Firmware Version: "));

  if (success)
  {
    Serial.println(versionStr);
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  if (!success)
  {
    clearString(versionStr);
  }

  return success;
}

bool BondedHM10::getRole(Role &role)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_ROLE, true, NULL, RESPONSE_OKGET, _responseStr);

  if (success)
  {
    char *responseCode = (char *)calloc(2, sizeof(char));

    parseCommandResponseValue(RESPONSE_OKGET, _responseStr, responseCode);

    if (responseCode[0] == '0')
    {
      role = Role::Peripheral;
    }
    else if (responseCode[0] == '1')
    {
      role = Role::Central;
    }
    else
    {
      success = false;
    }

    free(responseCode);
  }

#ifdef DEBUG
  Serial.print(F("Role: "));

  if (success)
  {
    switch (role)
    {
    case Role::Central:
      Serial.println(F("Central"));
      break;

    case Role::Peripheral:
      Serial.println(F("Peripheral"));
      break;
    }
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::setRole(const Role role)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_ROLE, false, (role == Role::Central ? "1" : "0"), RESPONSE_OKSET, _responseStr);

#ifdef DEBUG
  Serial.print(F("SET Role: "));

  if (success)
  {
    switch (role)
    {
    case Role::Central:
      Serial.println(F("Central"));
      break;

    case Role::Peripheral:
      Serial.println(F("Peripheral"));
      break;
    }
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::getBaudRate(BaudRate &baudRate)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_BAUDRATE, true, NULL, RESPONSE_OKGET, _responseStr);

  if (success)
  {
    char *responseCode = (char *)calloc(2, sizeof(char));

    parseCommandResponseValue(RESPONSE_OKGET, _responseStr, responseCode);

    if (responseCode[0] == '0')
    {
      baudRate = BaudRate::Baud_9600;
    }
    else if (responseCode[0] == '1')
    {
      baudRate = BaudRate::Baud_19200;
    }
    else if (responseCode[0] == '2')
    {
      baudRate = BaudRate::Baud_38400;
    }
    else if (responseCode[0] == '3')
    {
      baudRate = BaudRate::Baud_57600;
    }
    else if (responseCode[0] == '4')
    {
      baudRate = BaudRate::Baud_115200;
    }
    else if (responseCode[0] == '5')
    {
      baudRate = BaudRate::Baud_4800;
    }
    else if (responseCode[0] == '6')
    {
      baudRate = BaudRate::Baud_2400;
    }
    else if (responseCode[0] == '7')
    {
      baudRate = BaudRate::Baud_1200;
    }
    else if (responseCode[0] == '8')
    {
      baudRate = BaudRate::Baud_230400;
    }
    else
    {
      success = false;
    }

    free(responseCode);
  }

#ifdef DEBUG
  Serial.print(F("Baud Rate: "));

  if (success)
  {
    switch (baudRate)
    {
    case BaudRate::Baud_9600:
      Serial.println(F("9600"));
      break;

    case BaudRate::Baud_19200:
      Serial.println(F("19200"));
      break;

    case BaudRate::Baud_38400:
      Serial.println(F("38400"));
      break;

    case BaudRate::Baud_57600:
      Serial.println(F("57600"));
      break;

    case BaudRate::Baud_115200:
      Serial.println(F("115200"));
      break;

    case BaudRate::Baud_4800:
      Serial.println(F("4800"));
      break;

    case BaudRate::Baud_2400:
      Serial.println(F("2400"));
      break;

    case BaudRate::Baud_1200:
      Serial.println(F("1200"));
      break;

    case BaudRate::Baud_230400:
      Serial.println(F("23040"));
      break;
    }
  }
  else
  {
    Serial.print(F("FAILURE: Unknown baud rate number. "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::setBaudRate(const BaudRate baudRate)
{
  uint8_t baudRateInt = (uint8_t)baudRate;
  char *baudRateStr = (char *)calloc(2, sizeof(char));
  sprintf(baudRateStr, "%i", baudRateInt);

  bool success = sendCommandWithExpectedResponse(COMMAND_BAUDRATE, false, baudRateStr, RESPONSE_OKSET, _responseStr);

  free(baudRateStr);

#ifdef DEBUG
  Serial.print(F("SET Baud Rate: "));

  if (success)
  {
    switch (baudRate)
    {
    case BaudRate::Baud_9600:
      Serial.println(F("9600"));
      break;

    case BaudRate::Baud_19200:
      Serial.println(F("19200"));
      break;

    case BaudRate::Baud_38400:
      Serial.println(F("38400"));
      break;

    case BaudRate::Baud_57600:
      Serial.println(F("57600"));
      break;

    case BaudRate::Baud_115200:
      Serial.println(F("115200"));
      break;

    case BaudRate::Baud_4800:
      Serial.println(F("4800"));
      break;

    case BaudRate::Baud_2400:
      Serial.println(F("2400"));
      break;

    case BaudRate::Baud_1200:
      Serial.println(F("1200"));
      break;

    case BaudRate::Baud_230400:
      Serial.println(F("230400"));
      break;
    }
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::getWorkType(WorkType &workType)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_WORKTYPE, true, NULL, RESPONSE_OKGET, _responseStr);

  if (success)
  {
    char *responseCode = (char *)calloc(2, sizeof(char));

    parseCommandResponseValue(RESPONSE_OKGET, _responseStr, responseCode);

    if (responseCode[0] == '0')
    {
      workType = WorkType::AutoStart;
    }
    else if (responseCode[0] == '1')
    {
      workType = WorkType::ManualStart;
    }
    else
    {
      success = false;
    }

    free(responseCode);
  }

#ifdef DEBUG
  Serial.print(F("Work Type: "));

  if (success)
  {
    switch (workType)
    {
    case WorkType::AutoStart:
      Serial.println(F("Auto Start"));
      break;

    case WorkType::ManualStart:
      Serial.println(F("Manual Start"));
      break;
    }
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  delay(500); // Delay is apparently needed as the next AT command after this will fail if started within 0.5 seconds.
  return success;
}

bool BondedHM10::setWorkType(const WorkType workType)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_WORKTYPE, false, (workType == WorkType::AutoStart ? "0" : "1"), RESPONSE_OKSET, _responseStr);

#ifdef DEBUG
  Serial.print(F("SET Device Work Type: "));

  if (success)
  {
    switch (workType)
    {
    case WorkType::AutoStart:
      Serial.println(F("Auto Start"));
      break;

    case WorkType::ManualStart:
      Serial.println(F("Manual Start"));
      break;
    }
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  delay(500); // Delay is apparently needed as the next AT command after this will fail if started within 0.5 seconds.
  return success;
}

bool BondedHM10::getLastConnectedAddress(char *address)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_RADD, true, NULL, RADD_RESPONSE, _responseStr);

  if (success)
  {
    parseCommandResponseValue(RADD_RESPONSE, _responseStr, address);
  }

#ifdef DEBUG
  Serial.print(F("Last Connected Address: "));

  if (success)
  {
    Serial.println(address);
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  if (!success)
  {
    clearString(address);
  }

  return success;
}

bool BondedHM10::clearLastConnectedAddress()
{
  bool success = sendCommandWithExpectedResponse(COMMAND_CLEAR, false, NULL, CLEAR_RESPONSE, _responseStr);

#ifdef DEBUG
  if (success)
  {
    Serial.println(F("Last connected address cleared."));
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::getWhitelistEnabled(bool &enabled)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_WHITELISTENABLED, true, NULL, RESPONSE_OKGET, _responseStr);

  if (success)
  {
    char *responseCode = (char *)calloc(2, sizeof(char));

    parseCommandResponseValue(RESPONSE_OKGET, _responseStr, responseCode);

    if (responseCode[0] == '0')
    {
      enabled = false;
    }
    else if (responseCode[0] == '1')
    {
      enabled = true;
    }
    else
    {
      success = false;
    }

    free(responseCode);
  }

#ifdef DEBUG
  Serial.print(F("Whitelist Enabled: "));

  if (success)
  {
    Serial.println((enabled ? F("Yes") : F("No")));
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::setWhitelistEnabled(const bool enabled)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_WHITELISTENABLED, false, (enabled ? "1" : "0"), RESPONSE_OKSET, _responseStr);

#ifdef DEBUG
  Serial.print(F("SET Whitelist Enabled: "));

  if (success)
  {
    Serial.println((enabled ? F("Yes") : F("No")));
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::getWhitelistAddress(const WhitelistSlot slot, char *address)
{
  uint8_t slotInt = (uint8_t)slot;
  char *whitelistStr = (char *)calloc(3, sizeof(char));
  sprintf(whitelistStr, "%i?", slotInt);

  char *responseStr = (char *)calloc(strlen(WHITELIST_RESPONSE) + 3, sizeof(char));
  sprintf(responseStr, "%s%i?:", WHITELIST_RESPONSE, slotInt);

  bool success = sendCommandWithExpectedResponse(COMMAND_WHITELIST, true, whitelistStr, responseStr, _responseStr);

  free(whitelistStr);

  if (success)
  {
    parseCommandResponseValue(responseStr, _responseStr, address);
  }

  free(responseStr);

#ifdef DEBUG
  Serial.print(F("Whitelist Address: "));

  if (success)
  {
    switch (slot)
    {
    case WhitelistSlot::Slot1:
      Serial.print(F("(Slot1) "));
      break;

    case WhitelistSlot::Slot2:
      Serial.print(F("(Slot2) "));
      break;

    case WhitelistSlot::Slot3:
      Serial.print(F("(Slot3) "));
      break;
    }

    Serial.println(address);
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::setWhitelistAddress(const WhitelistSlot slot, const char *address)
{
  uint8_t slotInt = (uint8_t)slot;
  char *whitelistStr = (char *)calloc(14, sizeof(char));
  sprintf(whitelistStr, "%i%s", slotInt, address);

  bool success = sendCommandWithExpectedResponse(COMMAND_WHITELIST, false, whitelistStr, WHITELIST_RESPONSE, _responseStr);

  free(whitelistStr);

#ifdef DEBUG
  Serial.print(F("SET Whitelist Address: "));

  if (success)
  {
    switch (slot)
    {
    case WhitelistSlot::Slot1:
      Serial.print(F("(Slot1) "));
      break;

    case WhitelistSlot::Slot2:
      Serial.print(F("(Slot2) "));
      break;

    case WhitelistSlot::Slot3:
      Serial.print(F("(Slot3) "));
      break;
    }

    Serial.println(address);
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::getBondMode(BondMode &bondMode)
{
  bool success = sendCommandWithExpectedResponse(COMMAND_BONDMODE, true, NULL, RESPONSE_OKGET, _responseStr);

  if (success)
  {
    char *responseCode = (char *)calloc(2, sizeof(char));

    parseCommandResponseValue(RESPONSE_OKGET, _responseStr, responseCode);

    if (responseCode[0] == '0')
    {
      bondMode = BondMode::NoAuth;
    }
    else if (responseCode[0] == '1')
    {
      bondMode = BondMode::AuthNoPin;
    }
    else if (responseCode[0] == '2')
    {
      bondMode = BondMode::AuthWithPin;
    }
    else if (responseCode[0] == '3')
    {
      bondMode = BondMode::AuthAndBond;
    }
    else
    {
      success = false;
    }

    free(responseCode);
  }

#ifdef DEBUG
  Serial.print(F("Bond Mode: "));

  if (success)
  {
    switch (bondMode)
    {
    case BondMode::NoAuth:
      Serial.println(F("No Auth"));
      break;

    case BondMode::AuthNoPin:
      Serial.println(F("Auth with no PIN"));
      break;

    case BondMode::AuthWithPin:
      Serial.println(F("Auth with PIN"));
      break;

    case BondMode::AuthAndBond:
      Serial.println(F("Auth and Bond"));
      break;
    }
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

bool BondedHM10::setBondMode(const BondMode bondMode)
{
  uint8_t bondModeInt = (uint8_t)bondMode;
  char *bondModeStr = (char *)calloc(2, sizeof(char));
  sprintf(bondModeStr, "%i", bondModeInt);

  bool success = sendCommandWithExpectedResponse(COMMAND_BONDMODE, false, bondModeStr, RESPONSE_OKSET, _responseStr);

  free(bondModeStr);

#ifdef DEBUG
  Serial.print(F("SET Bond Mode: "));

  if (success)
  {
    switch (bondMode)
    {
    case BondMode::NoAuth:
      Serial.println(F("No Auth"));
      break;

    case BondMode::AuthNoPin:
      Serial.println(F("Auth with no PIN"));
      break;

    case BondMode::AuthWithPin:
      Serial.println(F("Auth with PIN"));
      break;

    case BondMode::AuthAndBond:
      Serial.println(F("Auth and Bond"));
      break;
    }
  }
  else
  {
    Serial.print(F("FAILURE: "));
    Serial.println(_responseStr);
  }

  Serial.flush();
#endif

  return success;
}

#ifdef DEBUG
void BondedHM10::printDeviceConfig()
{
  delay(500);

  char *configStr = (char *)calloc(32, sizeof(char));
  bool configBool = false;

  getDeviceName(configStr);
  getFirmwareVersion(configStr);

  BaudRate baudRate = BaudRate::Baud_Default;
  getBaudRate(baudRate);

  BondMode bondMode = BondMode::NoAuth;
  getBondMode(bondMode);

  getAddress(configStr);

  Role role = Role::Central;
  getRole(role);

  WorkType workType = WorkType::AutoStart;
  getWorkType(workType);

  getWhitelistEnabled(configBool);
  getWhitelistAddress(WhitelistSlot::DefaultSlot, configStr);

  free(configStr);
}
#endif

uint16_t BondedHM10::getFlashStringHelperLength(const __FlashStringHelper *content)
{
  PGM_P p = reinterpret_cast<PGM_P>(content);
  uint16_t length = 0;

  while (true)
  {
    unsigned char c = pgm_read_byte(p++);

    if (c == 0)
    {
      break;
    }

    length++;
  }

  return length;
}

void BondedHM10::writeFlashStringHelperContent(const __FlashStringHelper *content)
{
  PGM_P p = reinterpret_cast<PGM_P>(content);

  while (true)
  {
    uint8_t c = (uint8_t)pgm_read_byte(p++);

    if (c == 0 || !_stream->write(c))
    {
      break;
    }
  }
}

bool BondedHM10::writeEvent(uint16_t id, const uint8_t *content, const uint16_t length)
{
  bool success = true;

  if (_initialized && _connected)
  {
    if (length > MAX_CONTENT_BUFFER_SIZE)
    {
#ifdef DEBUG
      Serial.print(F("The length of event content provided ("));
      Serial.print(length);
      Serial.print(F(" bytes) surpasses the max length of "));
      Serial.print(MAX_CONTENT_BUFFER_SIZE);
      Serial.println(F(" bytes allowed for events."));
#endif

      success = false;
    }
    else
    {
      if (_dataTransmittedOutputPin >= 0)
      {
        startTransmissionTimer();
      }

      _stream->write(EVENT_PREFIX, PREFIX_LEN);
      _stream->write(lowByte(id));
      _stream->write(highByte(id));
      _stream->write(lowByte(length));
      _stream->write(highByte(length));
      _stream->write(content, length);
    }
  }
  else
  {
    success = false;
  }

  return success;
}

bool BondedHM10::writeEvent(uint16_t id, const char *content)
{
  return writeEvent(id, content, strlen(content));
}

bool BondedHM10::writeEvent(uint16_t id, const char *content, const uint16_t length)
{
  return writeEvent(id, (const uint8_t *)content, length);
}

bool BondedHM10::writeEvent(uint16_t id, const __FlashStringHelper *content)
{
  bool success = true;

  if (_initialized && _connected)
  {
    uint16_t length = getFlashStringHelperLength(content);

    if (length > MAX_CONTENT_BUFFER_SIZE)
    {
#ifdef DEBUG
      Serial.print(F("The length of event content provided ("));
      Serial.print(length);
      Serial.print(F(" bytes) surpasses the max length of "));
      Serial.print(MAX_CONTENT_BUFFER_SIZE);
      Serial.println(F(" bytes allowed for events."));
#endif

      success = false;
    }
    else
    {
      if (_dataTransmittedOutputPin >= 0)
      {
        startTransmissionTimer();
      }

      _stream->write(EVENT_PREFIX, PREFIX_LEN);
      _stream->write(lowByte(id));
      _stream->write(highByte(id));
      _stream->write(lowByte(length));
      _stream->write(highByte(length));
      writeFlashStringHelperContent(content);
    }
  }
  else
  {
    success = false;
  }

  return success;
}

void BondedHM10::setEventReceivedHandler(EventReceivedUInt8Delegate eventReceivedHandler)
{
  _eventReceivedUInt8Handler = eventReceivedHandler;
}

void BondedHM10::setEventReceivedHandler(EventReceivedCharDelegate eventReceivedHandler)
{
  _eventReceivedCharHandler = eventReceivedHandler;
}

bool BondedHM10::writeMessage(const uint8_t *content, const uint16_t length)
{
  bool success = true;

  if (_initialized && _connected)
  {
    if (length > MAX_CONTENT_BUFFER_SIZE)
    {
#ifdef DEBUG
      Serial.print(F("The length of message content provided ("));
      Serial.print(length);
      Serial.print(F(" bytes) surpasses the max length of "));
      Serial.print(MAX_CONTENT_BUFFER_SIZE);
      Serial.println(F(" bytes allowed for messages."));
#endif

      success = false;
    }
    else
    {
      if (_dataTransmittedOutputPin >= 0)
      {
        startTransmissionTimer();
      }

      _stream->write(MESSAGE_PREFIX, PREFIX_LEN);
      _stream->write(lowByte(length));
      _stream->write(highByte(length));
      _stream->write(content, length);
    }
  }
  else
  {
    success = false;
  }

  return success;
}

bool BondedHM10::writeMessage(const char *content)
{
  return writeMessage(content, strlen(content));
}

bool BondedHM10::writeMessage(const char *content, const uint16_t length)
{
  return writeMessage((const uint8_t *)content, length);
}

bool BondedHM10::writeMessage(const __FlashStringHelper *content)
{
  bool success = true;

  if (_initialized && _connected)
  {
    uint16_t length = getFlashStringHelperLength(content);

    if (length > MAX_CONTENT_BUFFER_SIZE)
    {
#ifdef DEBUG
      Serial.print(F("The length of message content provided ("));
      Serial.print(length);
      Serial.print(F(" bytes) surpasses the max length of "));
      Serial.print(MAX_CONTENT_BUFFER_SIZE);
      Serial.println(F(" bytes allowed for messages."));
#endif

      success = false;
    }
    else
    {
      if (_dataTransmittedOutputPin >= 0)
      {
        startTransmissionTimer();
      }

      _stream->write(MESSAGE_PREFIX, PREFIX_LEN);
      _stream->write(lowByte(length));
      _stream->write(highByte(length));
      writeFlashStringHelperContent(content);
    }
  }
  else
  {
    success = false;
  }

  return success;
}

void BondedHM10::setMessageReceivedHandler(MessageReceivedUInt8Delegate messageReceivedHandler)
{
  _messageReceivedUInt8Handler = messageReceivedHandler;
}

void BondedHM10::setMessageReceivedHandler(MessageReceivedCharDelegate messageReceivedHandler)
{
  _messageReceivedCharHandler = messageReceivedHandler;
}

size_t BondedHM10::write(uint8_t d)
{
  if (_dataTransmittedOutputPin >= 0)
  {
    startTransmissionTimer();
  }

  return _stream->write(d);
}

size_t BondedHM10::write(const uint8_t *buffer, size_t size)
{
  if (_dataTransmittedOutputPin >= 0)
  {
    startTransmissionTimer();
  }

  return _stream->write(buffer, size);
}

int BondedHM10::availableForWrite()
{
  return _stream->availableForWrite();
}

void BondedHM10::sendCommand_Internal(const char *command, const bool query, const char *param)
{
  const uint8_t commandLen = strlen(command);

  if (commandLen == 0)
  {
    // If command is an empty string, then the blank default "AT" command will be sent.
    _stream->write(COMMAND_AT, 2);
  }
  else
  {
    const uint8_t paramLen = strlen(param);

    memcpy(_commandStr, COMMAND_PREFIX, 3);
    memcpy(_commandStr + 3, command, commandLen);

    if (paramLen > 0)
    {
      memcpy(_commandStr + 3 + commandLen, param, paramLen);
    }

    if (query)
    {
      memcpy(_commandStr + 3 + commandLen + paramLen, QUERY, 1);
    }

    _stream->write(_commandStr, 3 + commandLen + paramLen + (query ? 1 : 0));
  }

  _stream->flush();
}

bool BondedHM10::sendCommandWithExpectedResponse(const char *command, const bool query, const char *param, const char *expectedResponse, char *actualResponse, const uint16_t timeout)
{
#ifdef DEBUG
#ifdef VERBOSE
  Serial.print(F("Sending Command: "));
  Serial.print(F("command = "));
  Serial.print(command);
  Serial.print(F(", query = "));
  Serial.print((query ? F("true") : F("false")));
  Serial.print(F(", param = "));
  Serial.print(param);
  Serial.print(F(", expectedResponse = "));
  Serial.print(expectedResponse);
  Serial.print(F(", timeout = "));
  Serial.println(timeout);
  Serial.flush();
#endif
#endif

  unsigned long startTime = millis();
  sendCommand_Internal(command, query, param);

  return waitForResponse_Internal(expectedResponse, actualResponse, timeout, startTime);
}

bool BondedHM10::sendCommandWithExpectedResponse(const char *command, const bool query, const char *param, const char *expectedResponse, char *actualResponse)
{
  return sendCommandWithExpectedResponse(command, query, param, expectedResponse, actualResponse, DEFAULT_COMMAND_TIMEOUT);
}

bool BondedHM10::waitForResponse_Internal(const char *expectedResponse, char *actualResponse, const uint16_t timeout, const unsigned long startTime)
{
  uint16_t availableLen = 0;
  uint8_t expectedResponseLen = 0;

  if (expectedResponse != NULL)
  {
    expectedResponseLen = strlen(expectedResponse);
  }

  while ((availableLen = _stream->available()) < expectedResponseLen)
  {
    if (millis() > startTime + timeout)
    {
      if (availableLen > 0)
      {
        for (uint8_t i = 0; i < availableLen; i++)
        {
          actualResponse[i] = (char)_stream->read();
        }
      }

      clearString(actualResponse, availableLen);

#ifdef DEBUG
#ifdef VERBOSE
      Serial.println(F("Receiving response timed out."));
#endif
#endif

      return false;
    }
  }

  // Read in the response data.
  uint16_t responseLen = 0;
  char responseChar;
  bool success = true;
  unsigned long currentTime = 0;

  availableLen = _stream->available();

  if (availableLen == 0)
  {
    currentTime = millis();

    if (currentTime < startTime + timeout)
    {
      delay((startTime + timeout) - currentTime);
      availableLen = _stream->available();
    }
  }

  while (availableLen > 0)
  {
    for (uint8_t i = 0; i < availableLen; i++)
    {
      responseChar = (char)_stream->read();
      actualResponse[responseLen] = responseChar;

      if (expectedResponseLen > 0 && responseLen < expectedResponseLen && responseChar != expectedResponse[responseLen])
      {
#ifdef DEBUG
#ifdef VERBOSE
        Serial.println(F("Unexpected character read."));
#endif
#endif

        success = false;
      }

      responseLen++;
    }

    currentTime = millis();

    if (currentTime < startTime + timeout)
    {
      delay((startTime + timeout) - currentTime);
    }

    availableLen = _stream->available();
  }

  clearString(actualResponse, responseLen);

  if (!success || responseLen < expectedResponseLen)
  {
    return false;
  }
  else
  {
    return true;
  }
}

bool BondedHM10::waitForResponse(const char *expectedResponse, char *actualResponse, const uint16_t timeout)
{
#ifdef DEBUG
#ifdef VERBOSE
  Serial.print(F("Waiting for Response: "));
  Serial.print(F("expectedResponse = "));
  Serial.print(expectedResponse);
  Serial.print(F(", timeout = "));
  Serial.println(timeout);
  Serial.flush();
#endif
#endif

  unsigned long startTime = millis();

  return waitForResponse_Internal(expectedResponse, actualResponse, timeout, startTime);
}

void BondedHM10::parseCommandResponseValue(const char *responsePrefix, const char *response, char *value)
{
  uint8_t responseLen = strlen(response);

  if (responseLen == 0)
  {
    clearString(value);
  }
  else
  {
    uint16_t valueIndex = 0;
    uint8_t prefixLen = 0;

    if (responsePrefix != NULL)
    {
      prefixLen = strlen(responsePrefix);
    }

    for (uint8_t i = prefixLen; i < responseLen; i++)
    {
      value[valueIndex] = response[i];
      valueIndex++;
    }

    clearString(value, valueIndex);
  }
}

void BondedHM10::clearString(char *str, const uint16_t startIndex)
{
  uint8_t strLen = strlen(str);

  if (strLen > 0 && startIndex < strLen)
  {
    str[startIndex] = 0;
  }
}

void BondedHM10::clearString(char *str)
{
  clearString(str, 0);
}

void BondedHM10::onConnect()
{
  bool isReconnected = _manuallyDisconnected;

  _connected = true;
  _connecting = false; // Done here as a safety precaution.
  _disconnected = false;
  _manuallyDisconnected = false;

  if (_connectedOutputPin > -1)
  {
    digitalWrite(_connectedOutputPin, HIGH);
  }

  if (_connectedHandler)
  {
    _connectedHandler(isReconnected);
  }
}

void BondedHM10::onDisconnect()
{
  _connected = false;
  _connecting = false; // Done here as a safety precaution.
  _disconnected = true;

  if (_connectedOutputPin > -1)
  {
    digitalWrite(_connectedOutputPin, LOW);
  }

  if (_disconnectedHandler)
  {
    _disconnectedHandler();
  }
}

void BondedHM10::detectAndHandleConnection()
{
  if (isConnected())
  {
    if (!_connected)
    {
#ifdef DEBUG
      Serial.println(F("Connect detected."));
#endif

      onConnect();
    }
  }
  else
  {
    if (_connected)
    {
#ifdef DEBUG
      Serial.println(F("Disconnect detected."));
#endif

      onDisconnect();
    }
    else if ((_role == Role::Central) && _autoReconnectEnabled && ((millis() - _lastConnectAttemptTimestamp) >= _autoReconnectTimeout) && !_manuallyDisconnected)
    {
      // If Auto-Reconnect is enabled AND we've waited the configured amount of time since the last connection attempt
      // AND the bluetooth device had not been manually disconnected, THEN attempt to reconnect to the last connected device.

#ifdef DEBUG
      Serial.println(F("Attempting to auto-reconnect to peripheral..."));
#endif

      bool timestampBeforeConnect = false;

      if (_autoReconnectTimeout > (CONNECTING_COMMAND_TIMEOUT + CONNECT_COMMAND_TIMEOUT))
      {
        timestampBeforeConnect = true;
        _lastConnectAttemptTimestamp = millis();
      }

      // Only attempt to reconnect if the last connected address matches the peripheral address.
      if (getLastConnectedAddress(_lastConnectedAddressStr))
      {
        startWork();

        if (strcmp(_lastConnectedAddressStr, _remoteAddress) != 0 || strcmp(_lastConnectedAddressStr, EMPTY_ADDRESS) == 0)
        {
          connectToPeripheral();
        }
      }

      if (!timestampBeforeConnect)
      {
        _lastConnectAttemptTimestamp = millis();
      }
    }
  }
}

void BondedHM10::detectAndHandleDisconnectReconnect()
{
  if (_disconnectReconnectInputPin > -1)
  {
    if (digitalRead(_disconnectReconnectInputPin) == LOW)
    {
      if (!_disconnected)
      {
        disconnect();
        _manuallyDisconnected = true;
      }
      else
      {
        reconnectToPeripheral();
      }

      delay(DISCONNECTRECONNECT_DEBOUNCE_TIMEOUT); // debounce
    }
  }
}

void BondedHM10::startTransmissionTimer()
{
  if (_transmissionTimer > 0 || (_transmissionTimerStoppedTimestamp > 0 && (millis() - _transmissionTimerStoppedTimestamp) < TRANSMISSION_TIMER_DEBOUNCE_TIMEOUT))
  {
    return;
  }

  noInterrupts(); // stop interrupts

  // We'll be using Timer0 which is already setup to trigger once 
  // per millisecond, so there is no need to change its prescalar.
  // We will need to change the compare value and enable the 
  // Compare A Match interrupt for Timer0.
  OCR0A = 249;                     // set timer compare value = (16*10^6) / (1000*64) - 1
  TIMSK0 |= bit(OCIE0A);           // enable timer Compare A Match interrupt

  interrupts(); // start interrupts again

  _transmissionTimer = 1;
  _transmissionTimerStoppedTimestamp = 0;

  if (_dataTransmittedOutputPin >= 0)
  {
    digitalWrite(_dataTransmittedOutputPin, HIGH);
  }
}

void BondedHM10::stopTransmissionTimer()
{
  if (_transmissionTimer == 0)
  {
    return;
  }

  noInterrupts(); // stop interrupts

  TIMSK0 &= ~bit(OCIE0A); // disable Timer0 Compare A Match interrupt

  interrupts(); // start interrupts again

  _transmissionTimer = 0;
  _transmissionTimerStoppedTimestamp = millis();

  if (_dataTransmittedOutputPin >= 0)
  {
    digitalWrite(_dataTransmittedOutputPin, LOW);
  }
}

void BondedHM10::handleTransmissionTimerInterrupt()
{
  // Ideally, we would have a static array of BondedHM10 objects, and simply 
  // iterate over those instances and run the following logic against it. Doing 
  // that would remove the need for this silly _instance variable. The static 
  // array would exclusively be maintained the constructor and destructor, along 
  // with resizing of the array and copying over of BondedHM10 pointers. The 
  // BondedHM10 class is fairly beefy, so it's best to keep the array small and 
  // resizing on demand to keep memory utilization as low as possible.

  if (_instance->_transmissionTimer > 0)
  {
    _instance->_transmissionTimer++;
  }

  if (_instance->_transmissionTimer >= TRANSMISSION_TIMER_DURATION)
  {
    _instance->stopTransmissionTimer();
  }
}

ISR(TIMER0_COMPA_vect)
{
  BondedHM10::handleTransmissionTimerInterrupt();
}