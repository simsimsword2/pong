/** MQTT Publish von Sensordaten */
#include "mbed.h"
#include "OLEDDisplay.h"

#if MBED_CONF_IOTKIT_HTS221_SENSOR == true
#include "HTS221Sensor.h"
#endif
#if MBED_CONF_IOTKIT_BMP180_SENSOR == true
#include "BMP180Wrapper.h"
#endif

#ifdef TARGET_K64F
#include "QEI.h"

//Use X2 encoding by default.
QEI wheel (MBED_CONF_IOTKIT_BUTTON2, MBED_CONF_IOTKIT_BUTTON3, NC, 624);
#endif

#include <MQTTClientMbedOs.h>
#include <MQTTNetwork.h>
#include <MQTTClient.h>
#include <MQTTmbed.h> // Countdown



// Topic's publish
char* topicTEMP = (char*) "iotkit_noob/sensor";
char* topicALERT = (char*) "iotkit_noob/alert";
char* topicBUTTON = (char*) "iotkit_noob/button";
char* topicENCODER = (char*) "iotkit_noob/encoder";
// Topic's subscribe
char* topicActors = (char*) "iotkit_noob/actors/#";
// MQTT Broker
char* hostname = (char*) "cloud.tbz.ch";
int port = 1883;
// MQTT Message
MQTT::Message message;
// I/O Buffer
char buf[1];

// UI
OLEDDisplay oled( MBED_CONF_IOTKIT_OLED_RST, MBED_CONF_IOTKIT_OLED_SDA, MBED_CONF_IOTKIT_OLED_SCL );



/** Hilfsfunktion zum Publizieren auf MQTT Broker */
void publish( MQTTNetwork &mqttNetwork, MQTT::Client<MQTTNetwork, Countdown> &client, char* topic )
{
    MQTT::Message message;    
    oled.cursor( 2, 0 );
    oled.printf( "Topi: %s\n", topic );
    oled.cursor( 3, 0 );    
    oled.printf( "Push: %s\n", buf );
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*) buf;
    message.payloadlen = strlen(buf)+1;
    client.publish( topic, message);  
}

/** Daten empfangen von MQTT Broker */
void messageArrived( MQTT::MessageData& md )
{
    float value;
    MQTT::Message &message = md.message;
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\n", message.qos, message.retained, message.dup, message.id);
    printf("Topic %.*s, ", md.topicName.lenstring.len, (char*) md.topicName.lenstring.data );
    printf("Payload %.*s\n", message.payloadlen, (char*) message.payload);

}

/** Hauptprogramm */
int main()
{
    uint8_t id;
    int encoder;
    
    oled.clear();
    oled.printf( "MQTTPublish\r\n" );
    oled.printf( "host: %s:%s\r\n", hostname, port );

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    oled.printf( "SSID: %s\r\n", MBED_CONF_APP_WIFI_SSID );
    
    // Connect to the network with the default networking interface
    // if you use WiFi: see mbed_app.json for the credentials
    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    if ( !wifi ) 
    {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }
    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect( MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2 );
    if ( ret != 0 ) 
    {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }    

    // TCP/IP und MQTT initialisieren (muss in main erfolgen)
    MQTTNetwork mqttNetwork( wifi );
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    printf("Connecting to %s:%d\r\n", hostname, port);
    int rc = mqttNetwork.connect(hostname, port);
    if (rc != 0)
        printf("rc from TCP connect is %d\r\n", rc); 

    // Zugangsdaten - der Mosquitto Broker ignoriert diese
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = (char*) wifi->get_mac_address(); // muss Eindeutig sein, ansonsten ist nur 1ne Connection moeglich
    data.username.cstring = (char*) wifi->get_mac_address(); // User und Password ohne Funktion
    data.password.cstring = (char*) "password";
    if ((rc = client.connect(data)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);           

    // MQTT Subscribe!
    client.subscribe( topicActors, MQTT::QOS0, messageArrived );
    printf("MQTT subscribe %s\n", topicActors );
    

    int oldnum = 0;

    wheel.reset();

    while   ( 1 ) 
    {

#ifdef TARGET_K64F
        // Encoder
        encoder = wheel.getPulses();

        if (encoder > oldnum) {
            sprintf( buf, "%d", 2);
            printf("%d\n\r", 2);
            publish( mqttNetwork, client, topicENCODER );
            sprintf( buf, "%d", 1);
            printf("%d\n\r", 1);
            publish( mqttNetwork, client, topicENCODER );
        }else if (encoder < oldnum) {
            sprintf( buf, "%d", 0);
            printf("%d\n\r", 0);
            publish( mqttNetwork, client, topicENCODER );
            sprintf( buf, "%d", 1);
            printf("%d\n\r", 1);
            publish( mqttNetwork, client, topicENCODER );
        }

#endif

        client.yield    ( 100 );                   // MQTT Client darf empfangen
        thread_sleep_for( 0 ); 
        
        oldnum = encoder;

    }

    // Verbindung beenden
    if ((rc = client.disconnect()) != 0)
        printf("rc from disconnect was %d\r\n", rc);

    mqttNetwork.disconnect(); 
     
}
