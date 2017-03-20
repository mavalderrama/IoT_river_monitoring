#include "add-on.h"
/*---------------------------------------------------------------------------*/
/*
 * MQTT broker IP address
 */
static const char *broker_ip1 = MQTT_DEMO_BROKER_IP_ADDR;
static uint16_t seq_nr_value1 = 0;
/*---------------------------------------------------------------------------*/
/*This function is used for turn off the leds on demand*/
void publish_led_off(void *d)
{
  leds_off(LEDS_GREEN);
}
/*---------------------------------------------------------------------------*/
/*This is the way we manage a received publish from cloud*/
void pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk,
            uint16_t chunk_len)
{
  printf("Pub Handler: topic='%s' (len=%u), chunk_len=%u\n", topic, topic_len,
         chunk_len);

  /* If we don't like the length, ignore */
  if (topic_len != 17 || chunk_len != 1)
  {
    printf("Incorrect topic or chunk len. Ignored\n");
    return;
  }

  if (strncmp(&topic[13], "leds", 4) == 0)
  {
    if (chunk[0] == '1')
    {
      leds_on(LEDS_RED);
      printf("Turning LED RED on!\n");
    }
    else if (chunk[0] == '0')
    {
      leds_off(LEDS_RED);
      printf("Turning LED RED off!\n");
    }
    return;
  }
}

/*---------------------------------------------------------------------------*/
/*Topic creation*/
int construct_pub_topic(void)
{
  //int len = snprintf(pub_topic, BUFFER_SIZE, "Specific Ubidots API requirement","Identifier Topic Source");
  //Take Care BUFFER_SIZE it's more larger than your topic string
  int len = snprintf(pub_topic, BUFFER_SIZE, "/v1.6/devices/%s", "nodo1");

  if (len < 0 || len >= BUFFER_SIZE)
  {
    printf("Pub Topic too large: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}

/*---------------------------------------------------------------------------*/
/*This function creates a topic in our mote*/
int construct_sub_topic(void)
{
  int len = snprintf(sub_topic, BUFFER_SIZE, "zolertia/cmd/%s",
                     conf.cmd_type); //In this particular case our topic is LEDS
  if (len < 0 || len >= BUFFER_SIZE)
  {
    printf("Sub Topic too large: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  printf("Subscription topic %s\n", sub_topic);

  return 1;
}

/*---------------------------------------------------------------------------*/
/*This function creates an ID based in the client IP*/
int construct_client_id(void)
{
  int len = snprintf(client_id, BUFFER_SIZE, "d:%02x%02x%02x%02x%02x%02x",
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if (len < 0 || len >= BUFFER_SIZE)
  {
    printf("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}

/*---------------------------------------------------------------------------*/
/*This function is a first stage config process started by "mqtt_demo_process"*/
void update_config(void)
{
  if (construct_client_id() == 0)
  {
    /* Fatal error. Client ID larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  if (construct_sub_topic() == 0)
  {
    /* Fatal error. Topic larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  if (construct_pub_topic() == 0)
  {
    /* Fatal error. Topic larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  /* Reset the counter */
  seq_nr_value1 = 0;

  state = STATE_INIT; //Initial state for FSM

  /*
   * Schedule next timer event ASAP
   * If we entered an error state then we won't do anything when it fires.
   * Since the error at this stage is a config error, we will only exit this
   * error state if we get a new config.
   */
  etimer_set(&publish_periodic_timer, 0);

  return;
}

/*---------------------------------------------------------------------------*/
int init_config(void)
{
  /* Populate configuration with default values */
  memset(&conf, 0, sizeof(mqtt_client_config_t));
  memcpy(conf.broker_ip, broker_ip1, strlen(broker_ip1));
  memcpy(conf.cmd_type, DEFAULT_SUBSCRIBE_CMD_TYPE, 4);
  conf.broker_port = DEFAULT_BROKER_PORT;
  conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;
  return 1;
}
/*---------------------------------------------------------------------------*/
/* Publish MQTT topic in IBM quickstart format */
void subscribe(void)
{
  mqtt_status_t status;

  status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0);

  printf("APP - Subscribing to %s\n", sub_topic);
  if (status == MQTT_STATUS_OUT_QUEUE_FULL)
  {
    printf("APP - Tried to subscribe but command queue was full!\n");
  }
}

/*---------------------------------------------------------------------------*/
/* This function handle the publish mechanism to the broker*/
void publish(void)
{
  int len = 0;                     //This is the actual size of output buffer
  int remaining = APP_BUFFER_SIZE; //This is the remaining size of the output buffer

  seq_nr_value1++;       //IDK
  buf_ptr = app_buffer; //This is the output buffer

  int elements_in_struct, size_struct;
  char *tempc = &ev_signals;//This is for navigation in the structure
  uint16_t *tempi = &ev_signals;//This is for navigation in the structure
  tempi = tempi + 10;//if we are located at the base addr given by the structure pointer, we need an offset of 10 integers of 16bit to reach the variable value.
  size_struct = sizeof(ev_signals);
  elements_in_struct = size_struct / (BUFFER_SIZE_TAGNAME + sizeof(uint16_t)); //take care about this type
  int i;
  for (i = 0; i < elements_in_struct; i++)//This FOR formats the JSON and copy the value given by the events structure.
  {
    if (i == 0)
    {
      if(elements_in_struct == 1)
        len = snprintf(buf_ptr, remaining, "{\"%s\":%u}", tempc, *tempi);
      else
        len = snprintf(buf_ptr, remaining, "{\"%s\":%u,", tempc, *tempi);
    }
    else if (i == elements_in_struct - 1)
      len = snprintf(buf_ptr, remaining, "\"%s\":%u}", tempc, *tempi);
    else
      len = snprintf(buf_ptr, remaining, "\"%s\":%u,", tempc, *tempi);
    remaining -= len; //Substracting the actual size of the output buffer
    buf_ptr += len;   //Positioning the pointer at the end of the buffer
    tempc = tempc + BUFFER_SIZE_TAGNAME + 2;//if we are positioned at the base addr, we need an offset of 22 bytes to reach the next variable name
    tempi = tempi + 11;//if we are positioned at the addr of the value addr, we need an offset of 11 integers of 16bits to reach the next variable value
  }

  /*If our buffer exceed the maximun size our publish process end*/
  if (len < 0 || len >= remaining)
  {
    printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    return;
  }
  //This function use the Radio to send our data
  mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer,
               strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);

  printf("APP - Publish to %s: %s\n", pub_topic, app_buffer);
}

/*---------------------------------------------------------------------------*/
/*This function connects to the broker using the IP provided*/
void connect_to_broker(void)
{
  /* Connect to MQTT server */
  mqtt_connect(&conn, conf.broker_ip, conf.broker_port,
               conf.pub_interval * 3);

  state = STATE_CONNECTING; //Actual state of the FSM
}



/*---------------------------------------------------------------------------*/
uint8_t ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr)
{
  uint16_t a;
  uint8_t len = 0;
  int i, f;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) {
        len += snprintf(&buf[len], buf_len - len, "::");
      }
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
        len += snprintf(&buf[len], buf_len - len, ":");
      }
      len += snprintf(&buf[len], buf_len - len, "%x", a);
    }
  }

  return len;
}


