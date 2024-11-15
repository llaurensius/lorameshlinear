#include <EEPROM.h>
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#include "DHT.h"
#include <Adafruit_Sensor.h>

#define DHTPIN 25     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11 // DHT 11

#define N_NODES 4     // Total number of nodes: N1, N2, N3, N4
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

// Declare a variable for the node ID
uint8_t nodeId;

RH_RF95 rf95(RFM95_CS, RFM95_INT); // RF95 driver with specified pins
RHMesh *manager; // Mesh manager
char buf[RH_MESH_MAX_MESSAGE_LEN]; // Buffer for messages

DHT dht(DHTPIN, DHTTYPE);

uint8_t sentCounter = 0;  // Counter for sent messages


void setup() {
    randomSeed(analogRead(0));
    Serial.begin(115200);
    while (!Serial); // Wait for Serial Monitor

    // Read node ID from EEPROM
    nodeId = EEPROM.read(EEPROM_ADDRESS);

    // Check if nodeId is invalid (1-4 range, corresponding to N1-N4)
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

        // Prepare message with temperature and humidity data
        sprintf(buf, "T:%.2f C H:%.2f %% Count: %.d", t, h, sentCounter + 1);
        Serial.print(F("Sending data to N4 via N2 and N3: "));
        Serial.println(buf);
        
        // Send message to N2 (next node)
        uint8_t errorN2 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 2);
        if (errorN2 != RH_ROUTER_ERROR_NONE) {
            Serial.print(F("Error sending to N2: "));
            Serial.println(errorN2);
        } else {
            Serial.println(F("Message sent to N2 successfully"));
            sentCounter++; // Increment only after successful send
            Serial.print(F("Sent Counter now: "));
            Serial.println(sentCounter); // Debug: Tampilkan counter
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
        Serial.print(F("Received from N"));
        Serial.print(from);
        Serial.print(F(": "));
        Serial.println(buf);

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
            // Forward to N3
            uint8_t errorN3 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 3);
            if (errorN3 != RH_ROUTER_ERROR_NONE) {
                Serial.print(F("Error sending to N3: "));
                Serial.println(errorN3);
            } else {
                Serial.println(F("Message forwarded to N3 successfully"));
            }

            // Display RSSI and SNR after forwarding
            rssi = rf95.lastRssi();
            snr = rf95.lastSNR();
            Serial.print(F("Node 2 - RSSI: "));
            Serial.print(rssi);
            Serial.print(F(" dBm, SNR: "));
            Serial.println(snr);
        } else if (nodeId == 3) {
            // Forward to N4
            uint8_t errorN4 = manager->sendtoWait((uint8_t *)buf, strlen(buf), 4);
            if (errorN4 != RH_ROUTER_ERROR_NONE) {
                Serial.print(F("Error sending to N4: "));
                Serial.println(errorN4);
            } else {
                Serial.println(F("Message forwarded to N4 successfully"));
            }

            // Display RSSI and SNR after forwarding
            rssi = rf95.lastRssi();
            snr = rf95.lastSNR();
            Serial.print(F("Node 3 - RSSI: "));
            Serial.print(rssi);
            Serial.print(F(" dBm, SNR: "));
            Serial.println(snr);
        } else if (nodeId == 4) {
            // If node 4 receives the data
            Serial.print(F("Data received at N4: "));
            Serial.println(buf);

            // Display RSSI and SNR for the message received at Node 4
            Serial.print(F("Node 4 - Received RSSI: "));
            Serial.print(rssi);
            Serial.print(F(" dBm, SNR: "));
            Serial.println(snr);
        }
    }

    delay(2000); // Delay before next transmission
}
