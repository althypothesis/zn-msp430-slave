/*** Defines ***/
#define STATE_WAIT_FOR_MY_ADR 0
#define STATE_GET_SENDER_ADR 1
#define STATE_GET_LEN 2
#define STATE_GATHER_DATA 3
#define STATE_PROCESS_DATA 4
#define STATE_PING_RESPONSE 5
#define STATE_STATUS_RESPONSE 6
#define STATE_INPUT_RESPONSE 7

const byte myId = 0x02;                             // Address on the bus
const byte serialNum[4] = { 0x11, 0x22, 0x33, 0x44 }; // Device serial number
const int baud = 19200;                             // Bus baud rate
const int responseTimeoutMs = 15; // Milliseconds before packet RX times out and goes back to listening for myId

/*** Global Variables ***/
short state;
int senderAdr;
int packetLen;
byte data[32];
int dataIndex;
long lastMillis;
bool ignorePacket;
int checksumErrors;
int timeoutErrors;
bool readSuccess = false;

/*** User Functions ***/
byte getSerialByte() {
	if (Serial.available())
	{
		readSuccess = true;
		return Serial.read();
	}
	else
	{
		readSuccess = false;
		return 255;
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

byte checksum(byte* checkData, int dataSize) {
	byte retVal = 0;
	for (int i = 0; i < dataSize; i++)
	{
		retVal ^= checkData[i];
	}
	return retVal;
}

void flashLED(int times) {
	for (int i = 0; i < times; i++)
	{
		digitalWrite(P1_0, 1);
		delay(50);
		digitalWrite(P1_0, 0);
		delay(150);
	}
}

/*** Setup ***/
void setup() {
	Serial.begin(baud);

	// GPIO setup
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
void loop() {
	switch (state) {
		case STATE_WAIT_FOR_MY_ADR: {
			digitalWrite(P1_0, 0);
			byte rxAdr = getSerialByte();
			if (rxAdr == myId && readSuccess) {
				state = STATE_GET_SENDER_ADR;
				lastMillis = millis();
			} else if (readSuccess) {
				state = STATE_GET_SENDER_ADR;
				lastMillis = millis();
				ignorePacket = true;
			}
			break;
		} case STATE_GET_SENDER_ADR: {
			digitalWrite(P1_0, 1);
			byte rxSenderAdr = getSerialByte();
			if (readSuccess) {
				senderAdr = rxSenderAdr;
				state = STATE_GET_LEN;
				lastMillis = millis();
			}
			break;
		} case STATE_GET_LEN: {
			byte rxLen = getSerialByte();
			if (readSuccess) {
				packetLen = rxLen;
				state = STATE_GATHER_DATA;
				lastMillis = millis();
			}
			break;
		} case STATE_GATHER_DATA: {
			byte rxDataByte = getSerialByte();
			if (readSuccess) {
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
			if (!ignorePacket) {
				byte dataToChecksum[35] = { };
				dataToChecksum[0] = myId;
				dataToChecksum[1] = senderAdr;
				dataToChecksum[2] = packetLen;
				for (int i = 3; i < packetLen + 3; i++)	{
					dataToChecksum[i] = data[i - 3];
				}
				byte chkSumResult = checksum(dataToChecksum, packetLen + 3);
				if (chkSumResult != 0) {
					checksumErrors++;
					resetState();
					break;
				}
			}

			if (!ignorePacket && data[0] == 0 && data[1] == 0) { // ping packet
				state = STATE_PING_RESPONSE;
				lastMillis = millis();
			}
			else if (!ignorePacket && data[0] == 0 && data[1] == 1) { // status packet
				state = STATE_STATUS_RESPONSE;
				lastMillis = millis();
			} else if (!ignorePacket && data[0] == 0 && data[1] == 2) { // input state packet
				state = STATE_INPUT_RESPONSE;
				lastMillis = millis();
			} else {
				resetState();
			}
			break;
		} case STATE_PING_RESPONSE: {
			//delay(1); // wow, we managed to make this work with no delay. Go team!
			byte txPingResponse[8] = { senderAdr, myId, 0x05, serialNum[0], serialNum[1], serialNum[2], serialNum[3], 0 };
			txPingResponse[7] = checksum(txPingResponse, sizeof(txPingResponse));

			for (unsigned int i = 0; i < sizeof(txPingResponse); i++) {
				Serial.write(txPingResponse[i]);
			}

			resetState();
			break;
		}
		case STATE_STATUS_RESPONSE: {
			byte txStatusResponse[6] = { senderAdr, myId, 0x03, checksumErrors, timeoutErrors, 0 };
			txStatusResponse[5] = checksum(txStatusResponse, sizeof(txStatusResponse));

			for (unsigned int i = 0; i < sizeof(txStatusResponse); i++) {
				Serial.write(txStatusResponse[i]);
			}

			resetState();
			break;
		}
		case STATE_INPUT_RESPONSE: {
			byte inputState;
			inputState |= digitalRead(P1_3)<<0;
			byte txInputResponse[5] = { senderAdr, myId, 0x02, inputState, 0 };

			txInputResponse[4] = checksum(txInputResponse, sizeof(txInputResponse));

			for (unsigned int i = 0; i < sizeof(txInputResponse); i++) {
				Serial.write(txInputResponse[i]);
			}

			resetState();
			break;
		}
	}

	//check for timeout
	if (state != STATE_WAIT_FOR_MY_ADR && (millis() - lastMillis) > responseTimeoutMs) {
		timeoutErrors++;
		resetState();
	}
}
