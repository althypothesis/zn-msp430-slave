/*** Defines ***/
#define STATE_WAIT_FOR_MY_ADR 0
#define STATE_GET_SENDER_ADR 1
#define STATE_GET_LEN 2
#define STATE_GATHER_DATA 3
#define STATE_PROCESS_DATA 4
#define STATE_PING_RESPONSE 5
#define STATE_STATUS_RESPONSE 6

const byte myId = 0x02;                             // Address on the bus
const byte serialNum[4] = {0x11, 0x22, 0x33, 0x44}; // Device serial number
const int baud = 19200;                             // Bus baud rate
const int responseTimeoutMs = 15;                    // Milliseconds before packet RX times out and goes back to listening for myId

/*** Global Variables ***/
short state;
int senderAdr;
int packetLen;
char data[32];
int dataIndex;
long lastMillis;
bool ignorePacket;
int checksumErrors;
int timeoutErrors;

/*** User Functions ***/
char getSerialByte() {
  if (Serial.available()) {
    return Serial.read();
  } else {
    return -1;
  }
}

void resetState() {
  state = STATE_WAIT_FOR_MY_ADR;
  senderAdr = 0;
  packetLen = 0;
  dataIndex = 0;
  ignorePacket = false;
  lastMillis = millis();
}

char checksum(char* checkData, int dataSize) {
  char retVal = 0;
  for (int i = 0; i < dataSize; i++) {
    retVal ^= checkData[i];
  }
  return retVal;
}

void flashLED(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(P1_0, 1);
    delay(50);
    digitalWrite(P1_0, 0);
    delay(150);
  }
}

/*** Setup ***/
void setup()
{
  Serial.begin(baud);

  // LED tomfoolery
  pinMode(P1_0, OUTPUT);
  pinMode(P1_3, INPUT_PULLUP);
  flashLED(1);

  Serial.write("Ready\r\n");

  state = STATE_WAIT_FOR_MY_ADR;
  senderAdr = 0;
  packetLen = 0;
  dataIndex = 0;
  ignorePacket = false;
  lastMillis = millis();
  checksumErrors = 0;
  timeoutErrors = 0;
}

/*** Main ***/
void loop()
{
  switch (state) {
    case STATE_WAIT_FOR_MY_ADR: {
        digitalWrite(P1_0, 0);
        char rxAdr = getSerialByte();
        if (rxAdr == myId) {
          state = STATE_GET_SENDER_ADR;
          lastMillis = millis();
        } else if (rxAdr >= 0) {
          state = STATE_GET_SENDER_ADR;
          lastMillis = millis();
          ignorePacket = true;
        }
        break;
      }
    case STATE_GET_SENDER_ADR: {
        digitalWrite(P1_0, 1);
        char rxSenderAdr = getSerialByte();
        if (rxSenderAdr >= 0) {
          senderAdr = rxSenderAdr;
          state = STATE_GET_LEN;
          lastMillis = millis();
        }
        break;
      }
    case STATE_GET_LEN: {
        char rxLen = getSerialByte();
        if (rxLen >= 0) {
          packetLen = rxLen;
          state = STATE_GATHER_DATA;
          lastMillis = millis();
        }
        break;
      }
    case STATE_GATHER_DATA: {
        char rxDataByte = getSerialByte();
        if (rxDataByte >= 0) {
          lastMillis = millis();
          data[dataIndex] = rxDataByte;
          dataIndex++;
          if (dataIndex == packetLen) {
            state = STATE_PROCESS_DATA;
          }
        }
        break;
      }
    case STATE_PROCESS_DATA: {
        // verify checksum        
        if(!ignorePacket) {
          char dataToChecksum[35];
          dataToChecksum[0] = myId;
          dataToChecksum[1] = senderAdr;
          dataToChecksum[2] = packetLen;
          for(int i = 3; i < 36; i++) {
            dataToChecksum[i] = data[i-3];
          }
          if (checksum(dataToChecksum, sizeof(dataToChecksum)) != 0) {
            checksumErrors++;
            flashLED(5);
            resetState();
            break;
          }
        }

        if (!ignorePacket && data[0] == 0 && data[1] == 0) { // ping packet
          state = STATE_PING_RESPONSE;
          lastMillis = millis();
        } else if (!ignorePacket && data[0] == 0 && data[1] == 1) { // status packet
          state = STATE_STATUS_RESPONSE;
          lastMillis = millis();
        } else {
          resetState();
        }
        break;
      }
    case STATE_PING_RESPONSE: {
        //delay(1); // wow, we managed to make this work with no delay. Go team!
        char txPingResponse[8] = { senderAdr, myId, 0x05, serialNum[0], serialNum[1], serialNum[2], serialNum[3], 0 };
        txPingResponse[7] = checksum(txPingResponse, sizeof(txPingResponse));

        for (unsigned int i = 0; i < sizeof(txPingResponse); i++) {
          Serial.write(txPingResponse[i]);
        }

        resetState();
        break;
      }
    case STATE_STATUS_RESPONSE: {
        char txStatusResponse[6] = { senderAdr, myId, 0x03, checksumErrors, timeoutErrors, 0 };
        txStatusResponse[5] = checksum(txStatusResponse, sizeof(txStatusResponse));

        for (unsigned int i = 0; i < sizeof(txStatusResponse); i++) {
          Serial.write(txStatusResponse[i]);
        }

        resetState();
        break;
      }
  }

  //check for timeout
  if (state != STATE_WAIT_FOR_MY_ADR && (millis() - lastMillis) > responseTimeoutMs) {
    timeoutErrors++;
    flashLED(3);
    resetState();
  }
}
