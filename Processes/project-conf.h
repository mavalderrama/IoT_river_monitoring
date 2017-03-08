/*
 */
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup SMAR2C
 * @{
 *
 * \file
 * Project specific configuration defines for the SMAR2C demo
 */
/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
/* User configuration */
#define DEFAULT_ORG_ID            "SMAR2C"

/*
 * If you have an IPv6 network or a NAT64-capable border-router:
 * test.mosquitto.org
 * IPv6 "2001:41d0:a:3a10::1"
 * NAT64 address "::ffff:5577:53c2" (85.119.83.194)
 *
 * To test locally you can use a mosquitto local broker in your host and use
 * i.e the fd00::1/64 address the Border router defaults to
 */
#define MQTT_DEMO_BROKER_IP_ADDR  "fd00::1"
/*---------------------------------------------------------------------------*/
/* Default configuration values */
#define DEFAULT_EVENT_TYPE_ID        "status"
#define DEFAULT_SUBSCRIBE_CMD_TYPE   "leds"
#define DEFAULT_BROKER_PORT          1883
#define DEFAULT_PUBLISH_INTERVAL     (45 * CLOCK_SECOND)
#define DEFAULT_KEEP_ALIVE_TIMER     60

/* Specific ZOUL platform values */
#define BUFFER_SIZE                  64
#define APP_BUFFER_SIZE              512

#undef CC2538_RF_CONF_CHANNEL
#define CC2538_RF_CONF_CHANNEL     26

/*---------------------------------------------------------------------------*/
#ifndef PROJECT_ROUTER_CONF_H_
#define PROJECT_ROUTER_CONF_H_

/* Comment this out to use Radio Duty Cycle (RDC) and save energy */
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC          nullrdc_driver

#undef IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID      0x5AC1

#ifndef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM          4
#endif

#undef UIP_CONF_BUFFER_SIZE
#define UIP_CONF_BUFFER_SIZE       256

#ifndef UIP_CONF_RECEIVE_WINDOW
#define UIP_CONF_RECEIVE_WINDOW    60
#endif

#ifndef WEBSERVER_CONF_CFS_CONNS
#define WEBSERVER_CONF_CFS_CONNS   2
#endif

#endif /* PROJECT_ROUTER_CONF_H_ */

/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE       32

#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
/** @} */

