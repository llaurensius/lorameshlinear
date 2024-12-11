#include <EEPROM.h>
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>

#define LED 13
#define N_NODES 5 // Total number of nodes: N1, N2, N3, N4, N5
#define EEPROM_ADDRESS 0 // EEPROM address to store node ID
#define GOSSIP_PROBABILITY 0.7 // 70% chance to forward the message

/*// Pin definitions for ESP32-MisRed
#define RFM95_CS 15    // Chip Select
#define RFM95_RST 26   // Reset
#define RFM95_INT 27   // DIO0
//*/
/// Pin definitions for ESP32-WROOM32
#define RFM95_CS 15    // Chip Select
#define RFM95_RST 26   // Reset
#define RFM95_INT 27   // DIO0
//*/
/*// Pin definitions for ESP32-TTGO
#define RFM95_CS 15    // Chip Select
#define RFM95_RST 26   // Reset
#define RFM95_INT 27   // DIO0
//*/
uint8_t nodeId; 
char buf[RH_MESH_MAX_MESSAGE_LEN]; // Buffer for messages
RH_RF95 rf95(RFM95_CS, RFM95_INT); // RF95 driver with specified pins
RHMesh *manager; // Mesh manager

void setup() {
    randomSeed(analogRead(0));
    pinMode(LED, OUTPUT);
    Serial.begin(115200);
    while (!Serial); // Wait for Serial Monitor

    // Read node ID from EEPROM
    nodeId = EEPROM.read(EEPROM_ADDRESS);
    if (nodeId < 1 || nodeId > N_NODES) {
        nodeId = 1; // Change this value for each node before uploading
        EEPROM.write(EEPROM_ADDRESS, nodeId);
        EEPROM.commit();
    }

    Serial.print(F("Initializing node "));
    Serial.println(nodeId);
    manager = new RHMesh(rf95, nodeId);
    
    if (!manager->init()) {
        Serial.println(F("Initialization failed"));
        return;
    }
    
    rf95.setFrequency(915.0);
    rf95.setTxPower(23, false);
    Serial.println(F("RF95 ready"));
}

void loop() {
    // Listen for incoming messages
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, 1000, &from)) {
        buf[len] = '\0'; // Null terminate string
        Serial.print(F("Received from N"));
        Serial.print(from);
        Serial.print(F(": "));
        Serial.println(buf);
        
        // Forward the message based on gossip probability
        if (random(0, 100) < GOSSIP_PROBABILITY * 100) {
            if (nodeId == 2) {
                sprintf(buf + len, " | Value N2: %d", random(100, 200));
                Serial.println(buf);
                uint8_t errorN3 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 3);
                if (errorN3 != RH_ROUTER_ERROR_NONE) {
                    Serial.print(F("Error sending to N3: "));
                    Serial.println(errorN3);
                } else {
                    Serial.println(F("Message forwarded to N3 successfully"));
                }
            }
            if (nodeId == 4) {
                sprintf(buf + len, " | Value N4: %d", random(200, 300));
                Serial.println(buf);
                uint8_t errorN3 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 3);
                if (errorN3 != RH_ROUTER_ERROR_NONE) {
                    Serial.print(F("Error sending to N3: "));
                    Serial.println(errorN3);
                } else {
                    Serial.println(F("Message forwarded to N3 successfully"));
                }
            }
        }
    }

    // Node 1 sends messages to Node 2
    if (nodeId == 1) {
        sprintf(buf, "Value N1: %d", random(0, 100));
        Serial.print(F("Sending to N2: "));
        Serial.println(buf);
        uint8_t errorN2 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 2);
        if (errorN2 != RH_ROUTER_ERROR_NONE) {
            Serial.print(F("Error sending to N2: "));
            Serial.println(errorN2);
        } else {
            Serial.println(F("Message sent to N2 successfully"));
        }
    }

    // Node 5 sends messages to Node 4
    if (nodeId == 5) {
        sprintf(buf, "Value N5: %d", random(300, 400));
        Serial.print(F("Sending to N4: "));
        Serial.println(buf);
        uint8_t errorN4 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 4);
        if (errorN4 != RH_ROUTER_ERROR_NONE) {
            Serial.print(F("Error sending to N4: "));
            Serial.println(errorN4);
        } else {
            Serial.println(F("Message sent to N4 successfully"));
        }
    }

    delay(2000); // Delay before next transmission
}
