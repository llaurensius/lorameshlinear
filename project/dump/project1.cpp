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

int freeMem() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
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

void loop() {
  // Node 1: Data sender
  if (nodeId == 1) {
    sprintf(buf, "Data from Node 1");
    manager->sendtoWait((uint8_t *)buf, strlen(buf), 2); // Send to Node 2
    delay(5000); // Wait before sending again
  }

  // Node 5: Data sender
  if (nodeId == 5) {
    sprintf(buf, "Data from Node 5");
    manager->sendtoWait((uint8_t *)buf, strlen(buf), 4); // Send to Node 4
    delay(5000); // Wait before sending again
  }

  // Node 2: Forwarder and sender
  if (nodeId == 2) {
    // Send data periodically
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime > 10000) { // Send every 10 seconds
      sprintf(buf, "Data from Node 2");
      manager->sendtoWait((uint8_t *)buf, strlen(buf), 3); // Send to Node 3
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
      manager->sendtoWait((uint8_t *)buf, strlen(buf), 3);
    }
  }

  // Node 4: Forwarder and sender
  if (nodeId == 4) {
    // Send data periodically
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime > 10000) { // Send every 10 seconds
      sprintf(buf, "Data from Node 4");
      manager->sendtoWait((uint8_t *)buf, strlen(buf), 3); // Send to Node 3
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
      manager->sendtoWait((uint8_t *)buf, strlen(buf), 3);
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
    }
  }

  // Update routing table
  updateRoutingTable();
  getRouteInfoString(buf, RH_MESH_MAX_MESSAGE_LEN);
  Serial.print(F("Routing info: "));
  Serial.println(buf);
}
