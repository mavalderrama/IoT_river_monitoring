#include "add-on.h"

/*---------------------------------------------------------------------------*/
/*Signal names for every process*/
process_event_t sLevel;
process_event_t sFlow;
process_event_t sTemp;
static struct mqtt_message *msg_ptr1 = 0;
/*---------------------------------------------------------------------------*/
/*Process and process name declaration*/
PROCESS(mqtt_demo_process, "MQTT Demo");
PROCESS(lvl_process, "Water level process");
PROCESS(buzzer_process, "Buzzer process");
PROCESS(flow_process, "Water flow process");
PROCESS(temp_process, "Temperature process");
AUTOSTART_PROCESSES(&mqtt_demo_process, &lvl_process, &buzzer_process, &flow_process, &temp_process); //This is used for start our process
/*---------------------------------------------------------------------------*/
/*MQTT event handler*/
/*In this function the FSM shows you the actual state of the connection with the broker*/
void mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
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
    msg_ptr1 = data;

    /* Implement first_flag in publish message? */
    if (msg_ptr1->first_chunk)
    {
      msg_ptr1->first_chunk = 0;
      printf("APP - Application received a publish on topic '%s'. Payload "
             "size is %i bytes. Content:\n\n",
             msg_ptr1->topic, msg_ptr1->payload_length);
    }

    pub_handler(msg_ptr1->topic, strlen(msg_ptr1->topic), msg_ptr1->payload_chunk, msg_ptr1->payload_length);
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
/*This is the FSM*/
void state_machine(void)
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
  update_config();

   /* These are variables to store strings for each sending process  */
  /*This is important to be set, because this values are the topics to be published with the JSON*/
  snprintf(ev_signals.ev_tag1, BUFFER_SIZE_TAGNAME, "Flow");//Flow is the topic
  snprintf(ev_signals.ev_tag2, BUFFER_SIZE_TAGNAME, "Level");//Level is the topic
  snprintf(ev_signals.ev_tag3, BUFFER_SIZE_TAGNAME, "Temp");//Temp is the topic

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
