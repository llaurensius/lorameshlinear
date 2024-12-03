#include <EEPROM.h>
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>

#define LED 13
#define N_NODES 5 // Total number of nodes: N1, N2, N3, N4, N5
#define EEPROM_ADDRESS 0 // EEPROM address to store node ID

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

// Declare a variable for the node ID
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
        // Set nodeId based on the specific node initially programmed
        // You can manually change the ID here for testing purposes
        // This code snippet is for Node 1
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
    // If the current node is N1, send a message to N2
    if (nodeId == 1) {
        // sprintf(buf, "Hello From Node 1"); // Prepare message for N2
        sprintf(buf, "Value N1: %d", randomValue1);
        Serial.print(F("Sending to N2..."));
        
        // Send message to N2
        uint8_t errorN2 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 2);
        if (errorN2 != RH_ROUTER_ERROR_NONE) {
            Serial.print(F("Error sending to N2: "));
            Serial.println(errorN2);
        } else {
            Serial.println(F("Message sent to N2 successfully"));
        }
    }
    if (nodeId == 5) {
        // sprintf(buf, "Hello From Node 5"); // Prepare message for N4
        sprintf(buf, "Value N5: %d", randomValue5);
        Serial.print(F("Sending to N4..."));
        
        // Send message to N4
        uint8_t errorN4 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 4);
        if (errorN4 != RH_ROUTER_ERROR_NONE) {
            Serial.print(F("Error sending to N4: "));
            Serial.println(errorN4);
        } else {
            Serial.println(F("Message sent to N4 successfully"));
        }
    }

    // Listen for incoming messages
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, 1000, &from)) {
        buf[len] = '\0'; // Null terminate string
        Serial.print(F("Received from N"));
        Serial.print(from);
        Serial.print(F(": "));
        Serial.println(buf);
        
        // Forward the message to the next node if necessary
        if (nodeId == 2) {
            // Prepare message to send to N3
            // sprintf(buf + len, " | Hello From Node 2"); // Append message from N2
            sprintf(buf + len, " | Value N2: %d", randomValue2); // Append message from N2
            uint8_t errorN3 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 3);
            if (errorN3 != RH_ROUTER_ERROR_NONE) {
                Serial.print(F("Error sending to N3: "));
                Serial.println(errorN3);
            } else {
                Serial.println(F("Message forwarded to N3 successfully"));
            }
        }
        if (nodeId == 4) {
            // Prepare message to send to N3
            // sprintf(buf + len, " | Hello From Node 4"); // Append message from N4
            sprintf(buf + len, " | Value N4: %d", randomValue4); // Append message from N2            
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
