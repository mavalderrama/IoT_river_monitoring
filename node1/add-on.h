#include "contiki-conf.h"
#include "rpl/rpl-private.h"
#include "mqtt.h"
#include "net/rpl/rpl.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "dev/leds.h"

#if CONTIKI_TARGET_ZOUL
#include "dev/adc-zoul.h"
#include "dev/zoul-sensors.h"
#else
#include "dev/adxl345.h"
#include "dev/i2cmaster.h"
#include "dev/tmp102.h"
#endif

#include <string.h>

/* Our header */
#include "smar2c.h"


/*---------------------------------------------------------------------------*/
/*
 * A timeout used when waiting for something to happen (e.g. to connect or to
 * disconnect)
 */
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)
/*---------------------------------------------------------------------------*/
/* Provide visible feedback via LEDS during various states */
/* When connecting to broker */
#define CONNECTING_LED_DURATION    (CLOCK_SECOND >> 2)

/* Each time we try to publish */
#define PUBLISH_LED_ON_DURATION    (CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/* Connections and reconnections */
#define RETRY_FOREVER              0xFF
#define RECONNECT_INTERVAL         (CLOCK_SECOND * 2)

/*
 * Number of times to try reconnecting to the broker.
 * Can be a limited number (e.g. 3, 10 etc) or can be set to RETRY_FOREVER
 */
#define RECONNECT_ATTEMPTS         RETRY_FOREVER
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)

/*---------------------------------------------------------------------------*/
/* States for MQTT FSM*/

#define STATE_INIT                    0
#define STATE_REGISTERED              1
#define STATE_CONNECTING              2
#define STATE_CONNECTED               3
#define STATE_PUBLISHING              4
#define STATE_DISCONNECTED            5
#define STATE_NEWCONFIG               6
#define STATE_CONFIG_ERROR         0xFE
#define STATE_ERROR                0xFF
/*---------------------------------------------------------------------------*/
#define CONFIG_EVENT_TYPE_ID_LEN     32
#define CONFIG_CMD_TYPE_LEN           8
#define CONFIG_IP_ADDR_STR_LEN       64
/*---------------------------------------------------------------------------*/
/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC        (CLOCK_SECOND >> 2)
#define NO_NET_LED_DURATION         (NET_CONNECT_PERIODIC >> 1)
/*---------------------------------------------------------------------------*/
#define BUFFER_SIZE_TAGNAME 20
/*---------------------------------------------------------------------------*/
struct timer connection_life;
uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/*Actual state variable*/
uint8_t state;
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topic.
 * Make sure they are large enough to hold the entire respective string
 */
char client_id[BUFFER_SIZE];
char pub_topic[BUFFER_SIZE]; //Topic to publish in MQTT Broker
char sub_topic[BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
struct mqtt_connection conn;
char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/

/*Timers Declaration*/
struct etimer publish_periodic_timer; //This timer triggers a publish to a topic periodically
struct ctimer ct;                     //Publish timer
struct etimer et_flow;                //Polling time for Flow process
struct etimer et_temp;                //Polling time for Temperature process
/*---------------------------------------------------------------------------*/
/**
 * \brief Data structure declaration for the MQTT client configuration
 */

typedef struct mqtt_client_config {
  //char event_type_id[CONFIG_EVENT_TYPE_ID_LEN];
  char broker_ip[CONFIG_IP_ADDR_STR_LEN];
  char cmd_type[CONFIG_CMD_TYPE_LEN];
  clock_time_t pub_interval;
  uint16_t broker_port;
} mqtt_client_config_t;

/*Configuration Structure*/
mqtt_client_config_t conf;

typedef struct signal_tag_tx
{
  char ev_tag1[BUFFER_SIZE_TAGNAME];
  uint16_t val1;
  char ev_tag2[BUFFER_SIZE_TAGNAME];
  uint16_t val2;
  char ev_tag3[BUFFER_SIZE_TAGNAME];
  uint16_t val3;
}signal_tag_tx_t;
/*---------------------------------------------------------------------------*/
/*Check this later*/
//TODO: Check this pointers for comment
char *buf_ptr;

/*---------------------------------------------------------------------------*/
/*This function is used for turn off the leds on demand*/
void publish_led_off(void *d);
/*This is the way we manage a received publish from cloud*/
void pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk,uint16_t chunk_len);
/*In this function the FSM shows you the actual state of the connection with the broker*/
void mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data);
/*Topic creation*/
int construct_pub_topic(void);
/*This function creates a topic in our mote*/
int construct_sub_topic(void);
/*This function creates an ID based in the client IP*/
int construct_client_id(void);
/*This function is a first stage config process started by "mqtt_demo_process"*/
void update_config(void);
/*Require comment*/
int init_config(void);
/* Publish MQTT topic in IBM quickstart format */
void subscribe(void);
/* This function handle the publish mechanism to the broker*/
void publish(void);
/*This function connects to the broker using the IP provided*/
void connect_to_broker(void);
/*This is the FSM*/
void state_machine(void);
/*Event signals structure*/
signal_tag_tx_t ev_signals;