#include <EEPROM.h>
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#include "DHT.h"
#include <Adafruit_Sensor.h>
#include <set>

#define DHTPIN 25     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11 // DHT 11

#define N_NODES 4     // Total number of nodes: N1, N2, N3, N4
#define EEPROM_ADDRESS 0 // EEPROM address to store node ID
#define MAX_RECEIVED_IDS 100 // Maximum number of IDs to store

/// Pin definitions for TTGO LoRa V1
#define RFM95_CS 18    // Chip Select
#define RFM95_RST 24   // Reset
#define RFM95_INT 26   // DIO0
//*/

/*// Pin definitions for ESP32-MisRed
#define RFM95_CS 15    // Chip Select
#define RFM95_RST 26   // Reset
#define RFM95_INT 27   // DIO0
//*/

// Declare a variable for the node ID
uint8_t nodeId;
uint8_t setNodeId = 3; // Change this value for each node before uploading

RH_RF95 rf95(RFM95_CS, RFM95_INT); // RF95 driver with specified pins
RHMesh *manager; // Mesh manager
char buf[RH_MESH_MAX_MESSAGE_LEN]; // Buffer for messages

DHT dht(DHTPIN, DHTTYPE);

uint8_t sentCounter = 0;  // Counter for sent messages

// Set to track received message IDs
static std::set<uint8_t> receivedIDs;

// Function to send messages with retry on failure
bool sendWithRetry(uint8_t destination, const char *message) {
    const int maxRetries = 5; // Max retry attempts
    int attempt = 0;
    uint8_t error;

    while (attempt < maxRetries) {
        error = manager->sendtoWait((uint8_t *)message, strlen(message), destination);
        
        // Check for errors
        if (error == RH_ROUTER_ERROR_NONE) {
            Serial.println(F("Message sent successfully"));
            return true; // Message sent successfully
        }

        // Handle specific errors
        Serial.print(F("Error sending to N"));
        Serial.print(destination);
        Serial.print(F(": "));
        Serial.println(error);
        
        attempt++;
        delay(1000); // Wait before retrying
    }
    
    Serial.println(F("Failed to send message after retries"));
    return false; // All attempts failed
}

// Function to periodically remove old IDs from receivedIDs set
void cleanOldIDs() {
    while (receivedIDs.size() > MAX_RECEIVED_IDS) {
        receivedIDs.erase(receivedIDs.begin()); // Remove the oldest ID
    }
}

bool listenBeforeTalk() {
    // Check if channel is busy
    if (rf95.available()) {
        Serial.println(F("Channel is busy, not sending."));
        return false; // Channel is busy
    }
    Serial.println(F("Channel is clear, ready to send."));
    return true; // Channel is clear
}

void setup() {
    randomSeed(analogRead(0));
    Serial.begin(115200);
    while (!Serial); // Wait for Serial Monitor

    // Read node ID from EEPROM
    nodeId = EEPROM.read(EEPROM_ADDRESS);

    // Check if nodeId is invalid (1-4 range, corresponding to N1-N4)
    if (nodeId < 1 || nodeId > N_NODES) {
        nodeId = setNodeId; 
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
    rf95.setFrequency(920.0);
    rf95.setTxPower(23, false);
    dht.begin();
    Serial.println(F("RF95 ready with DHT11 test!"));
}

void loop() {
    // Node 1 will read data from the DHT11 sensor
    if (nodeId == 1) {
        float h = dht.readHumidity();
        float t = dht.readTemperature();

        // Check if any reads failed
        if (isnan(h) || isnan(t)) {
            Serial.println(F("Failed to read from DHT sensor!"));
            return;
        }

        // Prepare message with ID and sensor data
        sprintf(buf, "ID:%d T:%.2f C H:%.2f %%", sentCounter, t, h);
        Serial.print(F("Node 1: send data to N4 via N2 and N3: "));
        Serial.println(buf);

        // Listen to talk before sending
        if (listenBeforeTalk()) {
            // Send message to N2 with retry
            if (sendWithRetry(2, buf)) {
                sentCounter++; // Increment only after successful send
            }
        }

        // Display RSSI and SNR after sending
        int16_t rssi = rf95.lastRssi();
        float snr = rf95.lastSNR();
        Serial.print(F("Node 1 - RSSI: "));
        Serial.print(rssi);
        Serial.print(F(" dBm, SNR: "));
        Serial.println(snr);
    }

    // Listen for incoming messages
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, 1000, &from)) {
        buf[len] = '\0'; // Null terminate string
        
        // Extract ID from received message
        int messageId;
        sscanf(buf, "ID:%d", &messageId); // Assuming the ID is in the format "ID:x"

        // Check if the message ID is new
        if (receivedIDs.find(messageId) == receivedIDs.end()) {
            // ID not found, process the message
            receivedIDs.insert(messageId); // Add ID to set
            Serial.print(F("Received from N"));
            Serial.print(from);
            Serial.print(F(": "));
            Serial.println(buf);

            // Clean old IDs
            cleanOldIDs(); 

            // Get and display RSSI and SNR for the received message
            int16_t rssi = rf95.lastRssi();
            float snr = rf95.lastSNR();
            Serial.print(F("Node "));
            Serial.print(nodeId);
            Serial.print(F(" - Received RSSI: "));
            Serial.print(rssi);
            Serial.print(F(" dBm, SNR: "));
            Serial.println(snr);

            // Forward the message to the next node if necessary
            if (nodeId == 2) {
                // Forward to N3 with retry mechanism
                if (!sendWithRetry(3, buf)) {
                    Serial.println(F("Failed to forward to N3 after retries"));
                }
            } else if (nodeId == 3) {
                // Forward to N4 with retry mechanism
                if (!sendWithRetry(4, buf)) {
                    Serial.println(F("Failed to forward to N4 after retries"));
                }
            } 
        } else {
            Serial.println(F("Duplicate message received, ignoring."));
        }
    }

    delay(2000); // Delay before next transmission
}
