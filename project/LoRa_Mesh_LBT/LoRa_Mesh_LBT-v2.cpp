#include <EEPROM.h>
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#include "DHT.h"
#include <Adafruit_Sensor.h>
#include <set>

#define DHTPIN 25     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11 // DHT 11

#define N_NODES 3     // Total number of nodes: N1, N2, N3, N4
#define EEPROM_ADDRESS 0 // EEPROM address to store node ID
#define MAX_RECEIVED_IDS 100 // Maximum number of IDs to store

//PIN DEFINITIONS
/*// Pin definitions for TTGO LoRa V1
#define RFM95_CS 18    // Chip Select
#define RFM95_RST 24   // Reset
#define RFM95_INT 26   // DIO0
//*/

/// Pin definitions for ESP32-WROOM32
#define RFM95_CS 5    // Chip Select
#define RFM95_RST 14   // Reset
#define RFM95_INT 2   // DIO0
//*/

/*// Pin definitions for ESP32-MisRed
#define RFM95_CS 15    // Chip Select
#define RFM95_RST 26   // Reset
#define RFM95_INT 27   // DIO0
//*/

uint8_t nodeId;
uint8_t setNodeId = 2; // Change this value for each node before uploading

RH_RF95 rf95(RFM95_CS, RFM95_INT); // RF95 driver with specified pins
RHMesh *manager; // Mesh manager
char buf[RH_MESH_MAX_MESSAGE_LEN]; // Buffer for messages

DHT dht(DHTPIN, DHTTYPE);

uint8_t sentCounter = 0;  // Counter for sent messages

static std::set<uint8_t> receivedIDs;

bool sendWithRetry(uint8_t destination, const char *message) {
    const int maxRetries = 5; // Max retry attempts
    int attempt = 0;
    uint8_t error;

    while (attempt < maxRetries) {
        if (!listenBeforeTalk()) {
            delay(1000); // Wait and retry if the channel is busy
            continue;
        }

        error = manager->sendtoWait((uint8_t *)message, strlen(message), destination);
        
        if (error == RH_ROUTER_ERROR_NONE) {
            Serial.println(F("Message sent successfully"));
            return true;
        }

        Serial.print(F("Error sending to N"));
        Serial.print(destination);
        Serial.print(F(": "));
        Serial.println(error);
        
        attempt++;
        delay(1000);
    }
    
    Serial.println(F("Failed to send message after retries"));
    return false;
}

void cleanOldIDs() {
    while (receivedIDs.size() > MAX_RECEIVED_IDS) {
        receivedIDs.erase(receivedIDs.begin());
    }
}

bool listenBeforeTalk() {
    if (rf95.available()) {
        Serial.println(F("Channel is busy, not sending."));
        return false;
    }
    Serial.println(F("Channel is clear, ready to send."));
    return true;
}

void setup() {
    randomSeed(analogRead(0));
    Serial.begin(115200);
    while (!Serial);

    nodeId = EEPROM.read(EEPROM_ADDRESS);

    if (nodeId < 1 || nodeId > N_NODES) {
        nodeId = setNodeId; 
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
    
    rf95.setFrequency(920.0);
    rf95.setTxPower(23, false);
    dht.begin();
    Serial.println(F("RF95 ready with DHT11 test!"));
}

void loop() {
    if (nodeId == 1) {
        float h = dht.readHumidity();
        float t = dht.readTemperature();

        if (isnan(h) || isnan(t)) {
            Serial.println(F("Failed to read from DHT sensor!"));
            return;
        }

        sprintf(buf, "ID:%d T:%.2f C H:%.2f %%", sentCounter, t, h);

        // Ensure channel is clear before sending
        if (listenBeforeTalk() && sendWithRetry(2, buf)) {
            sentCounter++;
        }

        int16_t rssi = rf95.lastRssi();
        float snr = rf95.lastSNR();
        Serial.print(F("Node 1 - RSSI: "));
        Serial.print(rssi);
        Serial.print(F(" dBm, SNR: "));
        Serial.println(snr);
    }

    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, 1000, &from)) {
        buf[len] = '\0';
        
        int messageId;
        sscanf(buf, "ID:%d", &messageId);

        if (receivedIDs.find(messageId) == receivedIDs.end()) {
            receivedIDs.insert(messageId);
            Serial.print(F("Received from N"));
            Serial.print(from);
            Serial.print(F(": "));
            Serial.println(buf);

            cleanOldIDs(); 

            int16_t rssi = rf95.lastRssi();
            float snr = rf95.lastSNR();
            Serial.print(F("Node "));
            Serial.print(nodeId);
            Serial.print(F(" - Received RSSI: "));
            Serial.print(rssi);
            Serial.print(F(" dBm, SNR: "));
            Serial.println(snr);

            if (nodeId == 2) {
                // Ensure channel is clear before forwarding
                if (listenBeforeTalk() && !sendWithRetry(3, buf)) {
                    Serial.println(F("Failed to forward to N3 after retries"));
                }
            } else if (nodeId == 3) {
                // Ensure channel is clear before forwarding
                if (listenBeforeTalk() && !sendWithRetry(4, buf)) {
                    Serial.println(F("Failed to forward to N4 after retries"));
                }
            } 
        } else {
            Serial.println(F("Duplicate message received, ignoring."));
        }
    }

    delay(2000);
}
