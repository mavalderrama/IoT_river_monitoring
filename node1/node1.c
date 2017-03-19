#include "add-on.h"
/*---------------------------------------------------------------------------*/
/*
 * MQTT broker IP address
 */
static const char *broker_ip = MQTT_DEMO_BROKER_IP_ADDR;
/*---------------------------------------------------------------------------*/
static struct timer connection_life;
static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/*Actual state variable*/
static uint8_t state;
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topic.
 * Make sure they are large enough to hold the entire respective string
 */
static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE]; //Topic to publish in MQTT Broker
static char sub_topic[BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
/*Timers Declaration*/
static struct etimer publish_periodic_timer; //This timer triggers a publish to a topic periodically
static struct ctimer ct;                     //Publish timer
static struct etimer et_flow;                //Polling time for Flow process
static struct etimer et_temp;                //Polling time for Temperature process
/*---------------------------------------------------------------------------*/
/*Configuration Structure*/
static mqtt_client_config_t conf;
/*---------------------------------------------------------------------------*/
/*Topic Name Declaration*/
char *vibracion = "Vibracion";
char *flujo = "Flujo";
char *nivel = "Nivel";
char *humedad = "Humedad Relativa";
char *temp = "Temperatura";
/*---------------------------------------------------------------------------*/
/*Signal names for every process*/
process_event_t sLevel;
process_event_t sFlow;
process_event_t sTemp;
signal_tag_tx_t ev_signals;
/*---------------------------------------------------------------------------*/
/*Process and process name declaration*/
PROCESS(mqtt_demo_process, "MQTT Demo");
PROCESS(lvl_process, "Water level process");
PROCESS(buzzer_process, "Buzzer process");
PROCESS(flow_process, "Water flow process");
PROCESS(temp_process, "Temperature process");
AUTOSTART_PROCESSES(&mqtt_demo_process, &lvl_process, &buzzer_process, &flow_process, &temp_process); //This is used for start our process
/*---------------------------------------------------------------------------*/
/*Check this later*/
//TODO: Check this pointers for comment
static char *buf_ptr;
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
/*this is the string we send to the broker in JSON format
 * if you need to modify this JSON, please validate the format with online tools or whatever.
*/
//char tx_json_string[] = "{\"%s\":%u,\"%s\":%u,\"%s\":%u}";//{"Topic1":value1,"Topic2":value2,"Topic3":value3}
/*---------------------------------------------------------------------------*/
/*This function is used for turn off the leds on demand*/
static void
publish_led_off(void *d)
{
  leds_off(LEDS_GREEN);
}
/*---------------------------------------------------------------------------*/
/*This is the way we manage a received publish from cloud*/
static void
pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk,
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
/*MQTT event handler*/
/*In this function the FSM shows you the actual state of the connection with the broker*/
static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  printf("We are here in mqtt_event\n");
  switch (event)
  {
  case MQTT_EVENT_CONNECTED:
  {
    printf("APP - Application has a MQTT connection\n");
    timer_set(&connection_life, CONNECTION_STABLE_TIME);
    state = STATE_CONNECTED;
    printf("state = STATE_CONNECTED\n");
    break;
  }
  case MQTT_EVENT_DISCONNECTED:
  {
    printf("APP - MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));
    state = STATE_DISCONNECTED;
    process_poll(&mqtt_demo_process);
    break;
  }
  case MQTT_EVENT_PUBLISH:
  {
    msg_ptr = data;

    /* Implement first_flag in publish message? */
    if (msg_ptr->first_chunk)
    {
      msg_ptr->first_chunk = 0;
      printf("APP - Application received a publish on topic '%s'. Payload "
             "size is %i bytes. Content:\n\n",
             msg_ptr->topic, msg_ptr->payload_length);
    }

    pub_handler(msg_ptr->topic, strlen(msg_ptr->topic), msg_ptr->payload_chunk, msg_ptr->payload_length);
    break;
  }
  case MQTT_EVENT_SUBACK:
  {
    printf("APP - Application is subscribed to topic successfully\n");
    break;
  }
  case MQTT_EVENT_UNSUBACK:
  {
    printf("APP - Application is unsubscribed to topic successfully\n");
    break;
  }
  case MQTT_EVENT_PUBACK:
  {
    printf("APP - Publishing complete.\n");
    break;
  }
  default:
    printf("APP - Application got a unhandled MQTT event: %i\n", event);
    break;
  }
}
/*---------------------------------------------------------------------------*/
/*Topic creation*/
static int
construct_pub_topic(void)
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
static int
construct_sub_topic(void)
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
static int
construct_client_id(void)
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
static void
update_config(void)
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
  seq_nr_value = 0;

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
static int
init_config()
{
  /* Populate configuration with default values */
  memset(&conf, 0, sizeof(mqtt_client_config_t));
  memcpy(conf.broker_ip, broker_ip, strlen(broker_ip));
  memcpy(conf.cmd_type, DEFAULT_SUBSCRIBE_CMD_TYPE, 4);
  conf.broker_port = DEFAULT_BROKER_PORT;
  conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;
  return 1;
}
/*---------------------------------------------------------------------------*/
/* Publish MQTT topic in IBM quickstart format */
static void
subscribe(void)
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
/* This function handle the publish mechanism to the broker
 * The parameters of this function are the names and values to publish.
*/
//TODO: Change the list of parameter for a structure.
//static void publish(uint16_t value1,char *event_name1,uint16_t value2,char *event_name2,uint16_t value3,char *event_name3)
static void publish()
{
  int len = 0;                     //This is the actual size of output buffer
  int remaining = APP_BUFFER_SIZE; //This is the remaining size of the output buffer

  seq_nr_value++;       //IDK
  buf_ptr = app_buffer; //This is the output buffer

  int elements_in_struct, size_struct;
  char *tempc = &ev_signals;
  uint16_t *tempi = &ev_signals;
  tempi = tempi + 10;
  size_struct = sizeof(ev_signals);
  elements_in_struct = size_struct / (BUFFER_SIZE_TAGNAME + sizeof(uint16_t)); //take care about this type
  //len = snprintf(buf_ptr, remaining, "{\"%s\":%u,\"%s\":%u,\"%s\":%u}",event_name1,value1,event_name2,value2,event_name3,value3);
  /*If you modify the line below please check the JSON format with online tools or whatever*/
  //len = snprintf(buf_ptr, remaining, tx_json_string,event_name1,value1,event_name2,value2,event_name3,value3);
    int i;
    for (i = 0; i < elements_in_struct; i++)
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
      tempc = tempc + BUFFER_SIZE_TAGNAME + 2;
      tempi = tempi + 11;
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
static void
connect_to_broker(void)
{
  /* Connect to MQTT server */
  mqtt_connect(&conn, conf.broker_ip, conf.broker_port,
               conf.pub_interval * 3);

  state = STATE_CONNECTING; //Actual state of the FSM
}
/*---------------------------------------------------------------------------*/
/*This is the FSM
 * Due to the lack of time, the input parameters are the values of our processes
*/
//TODO: Change the input parameters for an structure or a global thing.
/*static void
state_machine(uint16_t value1,char *event_name1,
uint16_t value2,char *event_name2,uint16_t value3,char *event_name3)*/
static void state_machine()
{
  switch (state)
  {
  case STATE_INIT:
    /* If we have just been configured register MQTT connection */
    mqtt_register(&conn, &mqtt_demo_process, client_id, mqtt_event,
                  MAX_TCP_SEGMENT_SIZE);

    conn.auto_reconnect = 0;
    connect_attempt = 1;

    state = STATE_REGISTERED;
  //printf("Init\n");

  /* Notice there is no "break" here, it will continue to the
     * STATE_REGISTERED
     */
  case STATE_REGISTERED:
    if (uip_ds6_get_global(ADDR_PREFERRED) != NULL)
    {
      /* Registered and with a public IP. Connect */
      printf("Registered. Connect attempt %u\n", connect_attempt);
      connect_to_broker();
    }
    else
    {
      leds_on(LEDS_GREEN);
      ctimer_set(&ct, NO_NET_LED_DURATION, publish_led_off, NULL);
    }
    etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
      return;
      break;
  case STATE_CONNECTING:
    leds_on(LEDS_GREEN);
    ctimer_set(&ct, CONNECTING_LED_DURATION, publish_led_off, NULL);
    /* Not connected yet. Wait */
    printf("Connecting (%u)\n", connect_attempt);
    break;

  case STATE_CONNECTED:
  /* Notice there's no "break" here, it will continue to subscribe */

  case STATE_PUBLISHING:
    /* If the timer expired, the connection is stable. */
    if (timer_expired(&connection_life))
    {
      /*
       * Intentionally using 0 here instead of 1: We want RECONNECT_ATTEMPTS
       * attempts if we disconnect after a successful connect
       */
      connect_attempt = 0;
    }

    if (mqtt_ready(&conn) && conn.out_buffer_sent)
    {
      /* CONNECTED. Publish NOW!!!!!!!!!!! */
      if (state == STATE_CONNECTED)
      {
        subscribe();
        state = STATE_PUBLISHING;
      }
      else
      {
        leds_on(LEDS_GREEN);
        printf("Publishing\n");
        ctimer_set(&ct, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
        //publish(value1,event_name1,value2,event_name2,value3,event_name3);
        publish();
      }
      etimer_set(&publish_periodic_timer, conf.pub_interval);

      /* Return here so we don't end up rescheduling the timer */
      return;
    }
    else
    {
      /*
       * Our publish timer fired, but some MQTT packet is already in flight
       * (either not sent at all, or sent but not fully ACKd).
       *
       * This can mean that we have lost connectivity to our broker or that
       * simply there is some network delay. In both cases, we refuse to
       * trigger a new message and we wait for TCP to either ACK the entire
       * packet after retries, or to timeout and notify us.
       */
      printf("Publishing... (MQTT state=%d, q=%u)\n", conn.state,
             conn.out_queue_full);
    }
    break;

  case STATE_DISCONNECTED:
    printf("Disconnected\n");
    if (connect_attempt < RECONNECT_ATTEMPTS ||
        RECONNECT_ATTEMPTS == RETRY_FOREVER)
    {
      /* Disconnect and backoff */
      clock_time_t interval;
      mqtt_disconnect(&conn);
      connect_attempt++;

      interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt : RECONNECT_INTERVAL << 3;

      printf("Disconnected. Attempt %u in %lu ticks\n", connect_attempt, interval);

      etimer_set(&publish_periodic_timer, interval);

      state = STATE_REGISTERED;
      return;
    }
    else
    {
      /* Max reconnect attempts reached. Enter error state */
      state = STATE_ERROR;
      printf("Aborting connection after %u attempts\n", connect_attempt - 1);
    }
    break;

  case STATE_CONFIG_ERROR:
    /* Idle away. The only way out is a new config */
    printf("Bad configuration.\n");
    return;

  case STATE_ERROR:
  default:
    leds_on(LEDS_GREEN);
    printf("Default case: State=0x%02x\n", state);
    return;
  }

  /* If we didn't return so far, reschedule ourselves */
  etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_demo_process, ev, data)
{
  PROCESS_BEGIN();
  //printf("MQTT Demo Process\n");

  if (init_config() != 1)
  {
    PROCESS_EXIT();
  }

  static uint16_t datum;

  /* These are variables to store strings for each sending process  */
  snprintf(ev_signals.ev_tag1, BUFFER_SIZE_TAGNAME, "Flow");
  snprintf(ev_signals.ev_tag2, BUFFER_SIZE_TAGNAME, "Level");
  snprintf(ev_signals.ev_tag3, BUFFER_SIZE_TAGNAME, "Temp");
  update_config();

  while (1)
  {
    PROCESS_YIELD();

    if ((ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) ||
        ev == PROCESS_EVENT_POLL || ev == sFlow || ev == sTemp || ev == sLevel)
    {
      datum = *((uint16_t *)data);
      if (ev == sFlow)
      {
        ev_signals.val1 = datum;
      }
      else if (ev == sLevel)
      {
        ev_signals.val2 = datum;
      }
      else if (ev == sTemp)
      {
        ev_signals.val3 = datum;
      }
      state_machine();
      //state_machine(prelevel,"Level",pretemp,"Temp",preflow,"Flow");
    }
  }

  PROCESS_END();
}

PROCESS_THREAD(lvl_process, ev, data)
{
  static uint8_t thresholdY,
      thresholdR;
  static uint16_t lvl_counter;

  /* Every process start with this macro, we tell the system this is the start
   * of the thread
   */
  PROCESS_BEGIN();
  /* Create a pointer to the data, as we are expecting a string we use "char" */
  //static char *parent;
  //parent = (char * )data;

  //printf ("Water level process started by %s\n", parent);

  /* We need to allocate a numeric process ID to our process */
  sLevel = process_alloc_event();

  lvl_counter = NORMAL_LEVEL;
  thresholdY = YELLOW_TRESHOLD;
  thresholdR = RED_TRESHOLD;

  /* Start the user button using the "SENSORS_ACTIVATE" macro */
  SENSORS_ACTIVATE(button_sensor);

  button_sensor.configure(BUTTON_SENSOR_CONFIG_TYPE_INTERVAL, CLOCK_SECOND);

  /* Send turn off buzzer */
  process_post(&buzzer_process, sLevel, &thresholdY);
  //leds_on (LEDS_GREEN);   /* normal level */

  /* And now we wait for the button_sensor process to inform us about the water
   * level. If the user button is clicked,the water level is decreased; if the user button is pressed for more than one second, the water level is increased
   */

  while (1)
  {
    PROCESS_YIELD();

    if (ev == sensors_event && data == &button_sensor)
    {
      if (button_sensor.value(BUTTON_SENSOR_VALUE_TYPE_LEVEL) ==
          BUTTON_SENSOR_PRESSED_LEVEL)
      {
        lvl_counter++;
        if (lvl_counter >= thresholdY)
        {
          leds_off(LEDS_GREEN);
          leds_on(LEDS_YELLOW);
        }
        if (lvl_counter >= thresholdR)
        {
          leds_off(LEDS_YELLOW);
          leds_on(LEDS_RED);
          /* Activate buzzer process */
          process_post(&buzzer_process, sLevel, &thresholdR);
        }
      }
    }
    else if (ev == button_press_duration_exceeded)
    {
      lvl_counter--;
      if (lvl_counter < thresholdR)
      {
        leds_off(LEDS_RED);
        leds_on(LEDS_YELLOW);
      }
      if (lvl_counter < thresholdY)
      {
        leds_off(LEDS_YELLOW);
        leds_on(LEDS_GREEN);
        //Deactivate buzzer process
        process_post(&buzzer_process, sLevel, &thresholdY);
      }
    }
    printf("Current water level = %u m\n", lvl_counter);
    /* Send level value to head node */
    process_post(&mqtt_demo_process, sLevel, &lvl_counter);
  }

  /* This is the end of the process, we tell the system we are done.  Even if
   * we won't reach this due to the "while(...)" we need to include it
   */
  PROCESS_END();
}

PROCESS_THREAD(buzzer_process, ev, data)
{
  PROCESS_BEGIN();
  /* Create a pointer to the data, as we are expecting a string we use "char" */
  //static char *parent;
  //parent = (char * )data;

  //printf ("Buzzer process started by %s\n", parent);

  /* This is a variable to store datum sent by other processes */
  static uint8_t datum;

  /* We tell the system the application will drive the pin */
  GPIO_SOFTWARE_CONTROL(BUZZER_PORT_BASE, BUZZER_PIN_MASK);

  /* And set as output, starting low */
  GPIO_SET_OUTPUT(BUZZER_PORT_BASE, BUZZER_PIN_MASK);
  GPIO_SET_PIN(BUZZER_PORT_BASE, BUZZER_PIN_MASK);

  /* Wait until processes send us a message */

  while (1)
  {
    /* This protothread waits for any event, we need to check of the event type
     * ourselves
     */
    PROCESS_YIELD();

    /* get datum */
    datum = *((uint8_t *)data);

    /* We are checking here for an event from level_process */
    if (ev == sLevel)
    {
      if (datum == YELLOW_TRESHOLD)
      {
        //printf ("Buzzer off\n");
        GPIO_CLR_PIN(BUZZER_PORT_BASE, BUZZER_PIN_MASK);
      }
      else if (datum == RED_TRESHOLD)
      {
        //printf ("Buzzer on\n");
        GPIO_SET_PIN(BUZZER_PORT_BASE, BUZZER_PIN_MASK);
      }
    }
    //printf ("Buzzer got %u from %s\n", datum, sender_p);
  }

  /* This is the end of the process, we tell the system we are done.  Even if
   * we won't reach this due to the "while(...)" we need to include it
   */
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(temp_process, ev, data)
{
  /* This is a variable to store the sample period */
  static uint8_t t_temp;

  /* This is a variable to store the temperature */
  static uint16_t temp;

  /*  The sensors are already started at boot */

  PROCESS_BEGIN();

  /* Create a pointer to the data, as we are expecting a string we use "char" */

  /* We need to allocate a numeric process ID to our process */
  sTemp = process_alloc_event();

  t_temp = TEMP_SPERIOD; /* temperature sampling period */
  /* Spin the timer */
  etimer_set(&et_temp, t_temp * CLOCK_SECOND);

  while (1)
  {

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_temp));

    /* Read the temperature sensor */
    temp = cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED);
    printf("temp = %d.%u C\n", temp / 1000, temp % 1000);

    /* Send level value to head node */
    process_post(&mqtt_demo_process, sTemp, &temp);
    /* Reset timer */
    etimer_reset(&et_temp);
  }

  PROCESS_END();
}

PROCESS_THREAD(flow_process, ev, data)
{
  /* This is a variable to store the sample period */
  static uint8_t t_flow;
  /* These are variables to store the samples */
  //  static uint16_t adc1;
  static uint16_t adc3;

  PROCESS_BEGIN();

  /* Create a pointer to the data, as we are expecting a string we use "char" */
  static char *parent;
  parent = (char *)data;

  printf("Water flow process started by %s\n", parent);

  /* We need to allocate a numeric process ID to our process */
  sFlow = process_alloc_event();

  /* The ADC zoul library configures the on-board enabled ADC channels, more
   * information is provided in the board.h file of the platform
   */
  //  adc_zoul.configure (SENSORS_HW_INIT, ZOUL_SENSORS_ADC1);
  adc_zoul.configure(SENSORS_HW_INIT, ZOUL_SENSORS_ADC3);

  /* Wait before starting the process */
  t_flow = FLOW_SPERIOD; /* flow sampling period */
  etimer_set(&et_flow, t_flow * CLOCK_SECOND);

  while (1)
  {
    /* This protothread waits until timeout
     */
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_flow));

    /* Read from ADC1 */
    /*    adc1 = adc_zoul.value(ZOUL_SENSORS_ADC1);
    printf ("ADC1 (water flow) = %u mV\n", adc1);*/

    /* Read from ADC3 */
    adc3 = adc_zoul.value(ZOUL_SENSORS_ADC3);
    printf("ADC1 (water flow) = %u mV\n", adc3);

    /* Send level value to head node */
    //    process_post ( &send2br_process, sFlow, &adc1 );
    process_post(&mqtt_demo_process, sFlow, &adc3);

    /* Reset timer */
    etimer_reset(&et_flow);
  }

  /* This is the end of the process, we tell the system we are done.  Even if
   * we won't reach this due to the "while(...)" we need to include it
   */
  PROCESS_END();
}
