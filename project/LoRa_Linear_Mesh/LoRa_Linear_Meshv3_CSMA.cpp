#include <EEPROM.h>
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>

#define LED 13
#define N_NODES 5 // Total number of nodes: N1, N2, N3, N4, N5
#define EEPROM_ADDRESS 0 // EEPROM address to store node ID
#define CSMA_WAIT_TIME 100 // Maximum wait time before retrying (in milliseconds)

/*// Pin definitions for TTGO LoRa V1
#define RFM95_CS 18    // Chip Select
#define RFM95_RST 24   // Reset
#define RFM95_INT 26   // DIO0
//*/

/// Pin definitions for ESP32-MisRed
#define RFM95_CS 15    // Chip Select
#define RFM95_RST 26   // Reset
#define RFM95_INT 27   // DIO0
//*/

/*// Pin definitions for ESP32-WROOM32
#define RFM95_CS 5    // Chip Select
#define RFM95_RST 14   // Reset
#define RFM95_INT 2  // DIO0
//*/

uint8_t nodeId; 
uint8_t randomValue1, randomValue2, randomValue4, randomValue5;

RH_RF95 rf95(RFM95_CS, RFM95_INT); // RF95 driver with specified pins
RHMesh *manager; // Mesh manager
char buf[RH_MESH_MAX_MESSAGE_LEN]; // Buffer for messages

void setup() {
    randomSeed(analogRead(0));
    pinMode(LED, OUTPUT);
    Serial.begin(115200);
    while (!Serial); // Wait for Serial Monitor

    // Read node ID from EEPROM
    nodeId = EEPROM.read(EEPROM_ADDRESS);

    // Check if nodeId is invalid (1-3 range, corresponding to N1-N3)
    if (nodeId < 1 || nodeId > N_NODES) {
        nodeId = 1; // Change this value for each node before uploading
        EEPROM.write(EEPROM_ADDRESS, nodeId); // Save the ID to EEPROM
        EEPROM.commit(); // Make sure the data is saved
    }

    Serial.print(F("Initializing node "));
    Serial.println(nodeId);

    // Initialize the manager with the RF95 driver and node ID
    manager = new RHMesh(rf95, nodeId);
    
    if (!manager->init()) {
        Serial.println(F("Initialization failed"));
        return;
    }
    
    // Configure RF95
    rf95.setFrequency(915.0);
    rf95.setTxPower(23, false);
    Serial.println(F("RF95 ready"));
}

void loop() {
    randomValue1 = random(0, 100);
    randomValue2 = random(100, 200);
    randomValue4 = random(200, 300);
    randomValue5 = random(300, 400);
    unsigned long timestamps = millis() / 1000;  // Timestamp dalam detik

    // CSMA implementation
    bool channelBusy = false;
    uint8_t len = sizeof(buf);
    uint8_t from;

    // Check if the channel is busy
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, 0, &from)) {
        channelBusy = true; // Channel is busy if we received a message
    }

    // If the channel is not busy, send messages
    if (!channelBusy) {
        if (nodeId == 1) {
            sprintf(buf, "Value N1: %d | timestamps: %lu", randomValue1, timestamps);
            Serial.print(F("Sending to N2..."));
            uint8_t errorN2 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 2);
            if (errorN2 != RH_ROUTER_ERROR_NONE) {
                Serial.print(F("Error sending to N2: "));
                Serial.println(errorN2);
            } else {
                Serial.println(F("Message sent to N2 successfully"));
            }
        }
        if (nodeId == 5) {
            sprintf(buf, "Value N5: %d | timestamps: %lu", randomValue5, timestamps);
            Serial.print(F("Sending to N4..."));
            uint8_t errorN4 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 4);
            if (errorN4 != RH_ROUTER_ERROR_NONE) {
                Serial.print(F("Error sending to N4: "));
                Serial.println(errorN4);
            } else {
                Serial.println(F("Message sent to N4 successfully"));
            }
        }
    } else {
        // If the channel is busy, wait for a random time before retrying
        Serial.println(F("Channel busy, waiting..."));
        delay(random(CSMA_WAIT_TIME, CSMA_WAIT_TIME * 2)); // Wait for a random time
    }

    // Listen for incoming messages
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, 1000, &from)) {
        buf[len] = '\0'; // Null terminate string
        Serial.print(F("Received from N"));
        Serial.print(from);
        Serial.print(F(": "));
        Serial.println(buf);
        
        // Forward the message to the next node if necessary
        if (nodeId == 2) {
            sprintf(buf + len, " | Value N2: %d | timestamps: %lu", randomValue2, timestamps);
            uint8_t errorN3 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 3);
            if (errorN3 != RH_ROUTER_ERROR_NONE) {
                Serial.print(F("Error sending to N3: "));
                Serial.println(errorN3);
            } else {
                Serial.println(F("Message forwarded to N3 successfully"));
            }
        }
        if (nodeId == 4) {
            sprintf(buf + len, " | Value N4: %d | timestamps: %lu", randomValue4, timestamps);
            uint8_t errorN3 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 3);
            if (errorN3 != RH_ROUTER_ERROR_NONE) {
                Serial.print(F("Error sending to N3: "));
                Serial.println(errorN3);
            } else {
                Serial.println(F("Message forwarded to N3 successfully"));
            }
        }
    }

    delay(2000); // Delay before next transmission
}
