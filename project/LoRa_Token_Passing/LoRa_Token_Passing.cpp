#include <EEPROM.h>
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>

#define LED 13
#define N_NODES 5 // Total number of nodes: N1, N2, N3, N4, N5
#define EEPROM_ADDRESS 0 // EEPROM address to store node ID

// Pin definitions for ESP32-MisRed
#define RFM95_CS 15    // Chip Select
#define RFM95_RST 26   // Reset
#define RFM95_INT 27   // DIO0

// Declare a variable for the node ID
uint8_t nodeId; 
bool hasToken = false; // Flag to check if the node has the token

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

    // Check if nodeId is invalid (1-5 range, corresponding to N1-N5)
    if (nodeId < 1 || nodeId > N_NODES) {
        // Set nodeId based on the specific node initially programmed
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

    // Set the initial token holder
    if (nodeId == 1 || nodeId == 5) {
        hasToken = true; // Node 1 and Node 5 start with the token
    }
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

        // Check if the message is a token
        if (strcmp(buf, "TOKEN") == 0) {
            hasToken = true; // Set the flag to true if token is received
            Serial.print(F("Node "));
            Serial.print(nodeId);
            Serial.println(F(" received the token."));
        }
    }

    // If the current node has the token, send a message
    if (hasToken) {
        if (nodeId == 1) {
            sprintf(buf, "Hello From Node 1"); // Prepare message for N2
            Serial.print(F("Sending to N2..."));
            uint8_t errorN2 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 2);
            if (errorN2 != RH_ROUTER_ERROR_NONE) {
                Serial.print(F("Error sending to N2: "));
                Serial.println(errorN2);
            } else {
                Serial.println(F("Message sent to N2 successfully"));
            }
        } else if (nodeId == 2) {
            sprintf(buf, "Hello From Node 2"); // Prepare message for N3
            Serial.print(F("Sending to N3..."));
            uint8_t errorN3 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 3);
            if (errorN3 != RH_ROUTER_ERROR_NONE) {
                Serial.print(F("Error sending to N3: "));
                Serial.println(errorN3);
            } else {
                Serial.println(F("Message sent to N3 successfully"));
            }
        } else if (nodeId == 5) {
            sprintf(buf, "Hello From Node 5"); // Prepare message for N4
            Serial.print(F("Sending to N4..."));
            uint8_t errorN4 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 4);
            if (errorN4 != RH_ROUTER_ERROR_NONE) {
                Serial.print(F("Error sending to N4: "));
                Serial.println(errorN4);
            } else {
                Serial.println(F("Message sent to N4 successfully"));
            }
        } else if (nodeId == 4) {
            sprintf(buf, "Hello From Node 4"); // Prepare message for N3
            Serial.print(F("Sending to N3..."));
            uint8_t errorN3 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 3);
            if (errorN3 != RH_ROUTER_ERROR_NONE) {
                Serial.print(F("Error sending to N3: "));
                Serial.println(errorN3);
            } else {
                Serial.println(F("Message sent to N3 successfully"));
            }
        }

        // After sending the message, pass the token to the next node
        uint8_t nextNode;
        if (nodeId == 1) {
            nextNode = 2; // Pass token to Node 2
        } else if (nodeId == 2) {
            nextNode = 3; // Pass token to Node 3
        } else if (nodeId == 5) {
            nextNode = 4; // Pass token to Node 4
        } else if (nodeId == 4) {
            nextNode = 3; // Pass token to Node 3
        } else {
            nextNode = 1; // Loop back to Node 1 if needed
        }

        sprintf(buf, "TOKEN"); // Prepare the token message
        Serial.print(F("Passing token to N"));
        Serial.println(nextNode);
        manager->sendtoWait((uint8_t *)buf, strlen(buf), nextNode);
        hasToken = false; // Reset the token flag
    }

    delay(2000); // Delay before next transmission
}
