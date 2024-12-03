#include <EEPROM.h>
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#define RH_HAVE_SERIAL
#define LED 13
#define N_NODES 5 // Total number of nodes

uint8_t nodeId; // Set this to 1, 2, 3, 4, or 5 for each node
uint8_t routes[N_NODES]; // Full routing table for mesh
int16_t rssi[N_NODES]; // Signal strength info

// Singleton instance of the radio driver
RH_RF95 rf95;

// Class to manage message delivery and receipt, using the driver declared above
RHMesh *manager;

// Message buffer
char buf[RH_MESH_MAX_MESSAGE_LEN];
uint8_t messageId = 0; // Unique message ID

int freeMem() {
  // Mengembalikan jumlah memori bebas yang tersedia
  return ESP.getFreeHeap();
}

void setup() {
  randomSeed(analogRead(0));
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  while (!Serial); // Wait for serial port to be available

  nodeId = EEPROM.read(0); // Read node ID from EEPROM
  if (nodeId > N_NODES) {
    Serial.print(F("EEPROM nodeId invalid: "));
    Serial.println(nodeId);
    nodeId = 1; // Default to Node 1 if invalid
  }
  Serial.print(F("Initializing node "));
  Serial.println(nodeId);

  manager = new RHMesh(rf95, nodeId);

  if (!manager->init()) {
    Serial.println(F("Init failed"));
  } else {
    Serial.println("Done");
  }
  rf95.setTxPower(23, false);
  rf95.setFrequency(915.0);
  rf95.setCADTimeout(500);

  Serial.println("RF95 ready");

  for (uint8_t n = 1; n <= N_NODES; n++) {
    routes[n - 1] = 0;
    rssi[n - 1] = 0;
  }

  Serial.print(F("Memory = "));
  Serial.println(freeMem());
}

const __FlashStringHelper* getErrorString(uint8_t error) {
  switch (error) {
    case 1: return F("Invalid length");
    case 2: return F("No route");
    case 3: return F("Timeout");
    case 4: return F("No reply");
    case 5: return F("Unable to deliver");
  }
  return F("Unknown");
}

void updateRoutingTable() {
  for (uint8_t n = 1; n <= N_NODES; n++) {
    RHRouter::RoutingTableEntry *route = manager->getRouteTo(n);
    if (n == nodeId) {
      routes[n - 1] = 255; // Self
    } else {
      routes[n - 1] = route->next_hop;
      if (routes[n - 1] == 0) {
        rssi[n - 1] = 0; // Reset RSSI if no route
      }
    }
  }
}

void getRouteInfoString(char *p, size_t len) {
  p[0] = '\0';
  strcat(p, "[");
  for (uint8_t n = 1; n <= N_NODES; n++) {
    strcat(p, "{\"n\":");
    sprintf(p + strlen(p), "%d", routes[n - 1]);
    strcat(p, ",");
    strcat(p, "\"r\":");
    sprintf(p + strlen(p), "%d", rssi[n - 1]);
    strcat(p, "}");
    if (n < N_NODES) {
      strcat(p, ",");
    }
  }
  strcat(p, "]");
}

void printNodeInfo(uint8_t node, char *s) {
  Serial.print(F("Node: "));
  Serial.print(F("{"));
  Serial.print(F("\""));
  Serial.print(node);
  Serial.print(F("\""));
  Serial.print(F(": "));
  Serial.print(s);
  Serial.println(F("}"));
}

bool sendWithCSMA(uint8_t *data, size_t len, uint8_t to) {
  // CSMA/CA: Check if the channel is clear before sending
  while (!rf95.available()) {
    // Wait for the channel to be clear
    delay(10);
  }
  
  // Send the data
  return manager->sendtoWait(data, len, to);
}

void loop() {
  // Node 1: Data sender
  if (nodeId == 1) {
    sprintf(buf, "ID:%d;Data from Node 1", messageId++);
    if (sendWithCSMA((uint8_t *)buf, strlen(buf), 2)) {
      Serial.print("Sent: ");
      Serial.println(buf);
    }
    delay(5000); // Wait before sending again
  }

  // Node 5: Data sender
  if (nodeId == 5) {
    sprintf(buf, "ID:%d;Data from Node 5", messageId++);
    if (sendWithCSMA((uint8_t *)buf, strlen(buf), 4)) {
      Serial.print("Sent: ");
      Serial.println(buf);
    }
    delay(5000); // Wait before sending again
  }

  // Node 2: Forwarder and sender
  if (nodeId == 2) {
    // Send data periodically
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime > 10000) { // Send every 10 seconds
      sprintf(buf, "ID:%d;Data from Node 2", messageId++);
      if (sendWithCSMA((uint8_t *)buf, strlen(buf), 3)) {
        Serial.print("Sent: ");
        Serial.println(buf);
      }
      lastSendTime = millis();
    }

    // Receive and forward data
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, 1000, &from)) {
      buf[len] = '\0'; // Null terminate string
      Serial.print("Received from Node ");
      Serial.print(from);
      Serial.print(": ");
      Serial.println(buf);
      // Forward to Node 3
      if (sendWithCSMA((uint8_t *)buf, strlen(buf), 3)) {
        Serial.print("Forwarded to Node 3: ");
        Serial.println(buf);
      }
    }
  }

  // Node 4: Forwarder and sender
  if (nodeId == 4) {
    // Send data periodically
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime > 10000) { // Send every 10 seconds
      sprintf(buf, "ID:%d;Data from Node 4", messageId++);
      if (sendWithCSMA((uint8_t *)buf, strlen(buf), 3)) {
        Serial.print("Sent: ");
        Serial.println(buf);
      }
      lastSendTime = millis();
    }

    // Receive and forward data
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, 1000, &from)) {
      buf[len] = '\0'; // Null terminate string
      Serial.print("Received from Node ");
      Serial.print(from);
      Serial.print(": ");
      Serial.println(buf);
      // Forward to Node 3
      if (sendWithCSMA((uint8_t *)buf, strlen(buf), 3)) {
        Serial.print("Forwarded to Node 3: ");
        Serial.println(buf);
      }
    }
  }

  // Node 3: Central hub
  if (nodeId == 3) {
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager->recvfromAckTimeout((uint8_t *)buf, &len, 1000, &from)) {
      buf[len] = '\0'; // Null terminate string
      Serial.print("Data from Node ");
      Serial.print(from);
      Serial.print(": ");
      Serial.println(buf);
      // Send acknowledgment back
      sprintf(buf, "ACK for ID:%s", buf + 3); // Extract ID from received message
      sendWithCSMA((uint8_t *)buf, strlen(buf), from); // Send ACK back to sender
    }
  }

  // Update routing table
  updateRoutingTable();
  getRouteInfoString(buf, RH_MESH_MAX_MESSAGE_LEN);
  Serial.print(F("Routing info: "));
  Serial.println(buf);
}
