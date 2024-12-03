#include <EEPROM.h>
#include <RHMesh.h>
#include <RH_RF95.h>

#define N_NODES 3  // Jumlah total node dalam jaringan
#define EEPROM_ADDRESS 0  // Alamat EEPROM untuk menyimpan ID node

// Pin untuk modul LoRa
/*//TTGO LoRa32
#define RFM95_CS 18
#define RFM95_RST 24
#define RFM95_INT 26
//*/

///ESP32-Wroom
#define RFM95_CS 5
#define RFM95_RST 14
#define RFM95_INT 2
//*/

int nodeId;  // ID node yang dibaca dari EEPROM
RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHMesh manager(rf95);
uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];

void setup() {
    Serial.begin(9600);
    nodeId = EEPROM.read(EEPROM_ADDRESS);  // Membaca ID node dari EEPROM
    if (nodeId < 1 || nodeId > N_NODES) {
        nodeId = 1;  // Set default ID jika tidak valid
        EEPROM.write(EEPROM_ADDRESS, nodeId);
        EEPROM.commit(); // Make sure the data is saved

    }
    Serial.print("Initializing node ");
    Serial.println(nodeId);

    manager.init();
    rf95.setFrequency(915.0);  // Atur frekuensi radio
    rf95.setTxPower(13);        // Atur daya transmisi
    Serial.println(F("RF95 ready"));
}

void loop() {
    unsigned long timestamp = millis() / 1000;  // Timestamp dalam detik

    // Node 1 mengirimkan data ke Node 2
    if (nodeId == 1) {
        String dataToSend = "ID: " + String(nodeId) + ", Data from Node " + String(nodeId) + ", Timestamp: " + String(timestamp);
        sendData(2, dataToSend);
    }

    // Node 2 menerima data dari Node 1 dan mengirimkan data ke Node 3
    if (nodeId == 2) {
        if (receiveData()) {
            // Menerima data dari Node 1
            String receivedData = String((char*)buf);
            Serial.print("Data diterima di Node 2: ");
            Serial.println(receivedData);

            // Mengirim data dari Node 2 ke Node 3
            String dataToSend = "ID: " + String(nodeId) + ", Data from Node " + String(nodeId) + ", Timestamp: " + String(timestamp);
            sendData(3, dataToSend);

            // Meneruskan data yang diterima dari Node 1 ke Node 3
            sendData(3, receivedData);
        }
    }

    // Node 3 menerima data dari Node 2
    if (nodeId == 3) {
        if (receiveData()) {
            Serial.print("Data diterima di Node 3: ");
            Serial.println((char*)buf);
        }
    }

    delay(1000);  // Delay untuk menghindari pengiriman berlebihan
}

void sendData(int targetNode, const String& data) {
    // Mengirim data ke node target
    Serial.print("Mengirim ke Node ");
    Serial.print(targetNode);
    Serial.print(": ");
    Serial.println(data);
    manager.sendtoWait((uint8_t*)data.c_str(), data.length(), targetNode);
}

bool receiveData() {
    // Menerima data dari node lain
    uint8_t len = sizeof(buf);
    if (manager.recvfromAck(buf, &len)) {
        Serial.print("Diterima: ");
        Serial.println((char*)buf);
        return true;
    }
    return false;
}
