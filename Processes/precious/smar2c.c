/*
 * Copyright (c) 2016, Zolertia - http://www.zolertia.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/*---------------------------------------------------------------------------*/
/**
 * \file
 *         SMAR2C demo
 * \author
 *         5AC1 team
 *          J. Echaiz
 *          A. Hoyos
 *          A. Rudqvist
 *          E. Tamura
 *          M. Valderrama
 */
/*---------------------------------------------------------------------------*/
/* This is the main contiki header, it should be included always */
#include "contiki.h"

/* This is the standard input/output C library, used to print information to the
 * console, amongst other features
 */
#include <stdio.h>

/* This includes the user button header */
#include "dev/button-sensor.h"

/* The on-board LEDs header */
#include "dev/leds.h"

/* These header files allows to use the ADC and on-board sensors */
#include "dev/adc-zoul.h"
#include "dev/zoul-sensors.h"

/* The GPIO header */
#include "dev/gpio.h"

/* The event timer header */
#include "sys/etimer.h"

#include "net/linkaddr.h"

/* Our header */
#include "smar2c.h"

/* Event timers used in each process */
static struct etimer et_flow;
static struct etimer et_temp;

/*---------------------------------------------------------------------------*/
/* We create events to be used to communicate with send2br process */
process_event_t sLevel;
process_event_t sFlow;
process_event_t sTemp;

/*---------------------------------------------------------------------------*/
/* We first declare the name or our processes, and create a friendly printable
 * name
 */
PROCESS (lvl_process, "Water level process");
PROCESS (flow_process, "Water flow process");
PROCESS (temp_process, "Temperature process");
PROCESS (send2br_process, "Sender process");
PROCESS (buzzer_process, "Buzzer process");

/* And we tell the system to start these processes as soon as the device is done
 * initializing
 */
AUTOSTART_PROCESSES (&lvl_process,&flow_process,&temp_process,&send2br_process,&buzzer_process);

/*---------------------------------------------------------------------------*/
/* We declared the processes in the above step, now we are implementing the
 * processes.  A process communicates with other processes and, thats why we
 * include as arguments "ev" (event type) and "data" (information associated
 * with the event, so when we receive a message from another process, we can
 * check which type of event is, and depending of the type what data should I
 * receive
 */
PROCESS_THREAD (lvl_process, ev, data)
{
  static uint8_t  thresholdY,
                  thresholdR;
  static uint16_t lvl_counter;

  /* Every process start with this macro, we tell the system this is the start
   * of the thread
   */
  PROCESS_BEGIN ();
  /* Create a pointer to the data, as we are expecting a string we use "char" */
  static char *parent;
  parent = (char * )data;

  printf ("Water level process started by %s\n", parent);

  /* We need to allocate a numeric process ID to our process */
  sLevel = process_alloc_event ();

  lvl_counter = NORMAL_LEVEL;
  thresholdY = YELLOW_TRESHOLD;
  thresholdR = RED_TRESHOLD;

  /* Start the user button using the "SENSORS_ACTIVATE" macro */
  SENSORS_ACTIVATE (button_sensor);

  button_sensor.configure (BUTTON_SENSOR_CONFIG_TYPE_INTERVAL, CLOCK_SECOND);

  /* Send turn off buzzer */
  process_post ( &buzzer_process, sLevel, &thresholdY );
  leds_on (LEDS_GREEN);   /* normal level */

  /* And now we wait for the button_sensor process to inform us about the water
   * level. If the user button is clicked,the water level is decreased; if the user button is pressed for more than one second, the water level is increased
   */

  while (1)
  {
    PROCESS_YIELD ();

    if (ev == sensors_event && data == &button_sensor)
    {
      if (button_sensor.value(BUTTON_SENSOR_VALUE_TYPE_LEVEL) ==
                              BUTTON_SENSOR_PRESSED_LEVEL)
      {
        lvl_counter++;
        if (lvl_counter >= thresholdY)
        {
          leds_off (LEDS_GREEN);
          leds_on (LEDS_YELLOW);
        }
        if (lvl_counter >= thresholdR)
        {
          leds_off (LEDS_YELLOW);
          leds_on (LEDS_RED);
          /* Activate buzzer process */
          process_post ( &buzzer_process, sLevel, &thresholdR );
        }
      }
    }
    else if (ev == button_press_duration_exceeded)
    {
      lvl_counter--;
      if (lvl_counter < thresholdR)
      {
        leds_off (LEDS_RED);
        leds_on (LEDS_YELLOW);
      }
      if (lvl_counter < thresholdY)
      {
        leds_off (LEDS_YELLOW);
        leds_on (LEDS_GREEN);
        /* Deactivate buzzer process */
        process_post ( &buzzer_process, sLevel, &thresholdY );
      }
    }
    printf ( "Current water level = %u m\n", lvl_counter );
    /* Send level value to head node */
    process_post ( &send2br_process, sLevel, &lvl_counter );
  }

  /* This is the end of the process, we tell the system we are done.  Even if
   * we won't reach this due to the "while(...)" we need to include it
   */
  PROCESS_END ();
}

PROCESS_THREAD (flow_process, ev, data)
{
  /* This is a variable to store the sample period */
  static uint8_t  t_flow;
  /* These are variables to store the samples */
//  static uint16_t adc1;
  static uint16_t adc3;

  PROCESS_BEGIN ();

  /* Create a pointer to the data, as we are expecting a string we use "char" */
  static char *parent;
  parent = (char * )data;

  printf ("Water flow process started by %s\n", parent);

  /* We need to allocate a numeric process ID to our process */
  sFlow = process_alloc_event ();

  /* The ADC zoul library configures the on-board enabled ADC channels, more
   * information is provided in the board.h file of the platform
   */
//  adc_zoul.configure (SENSORS_HW_INIT, ZOUL_SENSORS_ADC1);
  adc_zoul.configure (SENSORS_HW_INIT, ZOUL_SENSORS_ADC3);

  /* Wait before starting the process */
  t_flow = FLOW_SPERIOD; /* flow sampling period */
  etimer_set (&et_flow, t_flow * CLOCK_SECOND);

  while (1)
  {
    /* This protothread waits until timeout
     */
    PROCESS_WAIT_EVENT_UNTIL (etimer_expired(&et_flow));

    /* Read from ADC1 */
/*    adc1 = adc_zoul.value(ZOUL_SENSORS_ADC1);
    printf ("ADC1 (water flow) = %u mV\n", adc1);*/

    /* Read from ADC3 */
    adc3 = adc_zoul.value(ZOUL_SENSORS_ADC3);
    printf ("ADC1 (water flow) = %u mV\n", adc3);

/* Send level value to head node */
//    process_post ( &send2br_process, sFlow, &adc1 );
    process_post ( &send2br_process, sFlow, &adc3 );

    /* Reset timer */
    etimer_reset (&et_flow);
  }

  /* This is the end of the process, we tell the system we are done.  Even if
   * we won't reach this due to the "while(...)" we need to include it
   */
  PROCESS_END();
}

PROCESS_THREAD (temp_process, ev, data)
{
  /* This is a variable to store the sample period */
  static uint8_t t_temp;

  /* This is a variable to store the temperature */
  static uint16_t temp;

  /*  The sensors are already started at boot */

  PROCESS_BEGIN ();

  /* Create a pointer to the data, as we are expecting a string we use "char" */
  static char *parent;
  parent = (char * )data;

  printf ("Temperature process started by %s\n", parent);

  /* We need to allocate a numeric process ID to our process */
  sTemp = process_alloc_event ();

  t_temp = TEMP_SPERIOD; /* temperature sampling period */
  /* Spin the timer */
  etimer_set (&et_temp, t_temp * CLOCK_SECOND);

  while (1)
  {

    PROCESS_WAIT_EVENT_UNTIL (etimer_expired(&et_temp));

    /* Read the temperature sensor */
    temp = cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED);
    printf("temp = %d.%u C\n", temp / 1000, temp % 1000);

    /* Send level value to head node */
    process_post ( &send2br_process, sTemp, &temp );
    /* Reset timer */
    etimer_reset (&et_temp);
  }

  PROCESS_END();
}

PROCESS_THREAD(send2br_process, ev, data)
{
  PROCESS_BEGIN();
  /* Create a pointer to the data, as we are expecting a string we use "char" */
  static char *parent;
  parent = (char * )data;

  printf ("Sender process started by %s\n", parent);

  /* This is a variable to store datum sent by other processes */
  static uint16_t datum;

  /* These are variables to store strings for each sending process  */
  static char *level_p = "water level process";
  static char *flow_p = "water flow process";
  static char *temp_p = "temp process";

  /* This variable stores a string pointer */
  static char *sender_p;

  static uint8_t i;

  /* Wait until processes send us a message */

  while (1)
  {
    /* This protothread waits for any event, we need to check of the event type
     * ourselves
     */
    PROCESS_YIELD();
    /* get datum */
    datum = *((uint16_t *)data);

    /* We are checking here for an event from flow_process */
    if (ev == sFlow)
      sender_p = flow_p;
    else if (ev == sTemp)
      sender_p = temp_p;
    else if (ev == sLevel)
      sender_p = level_p;

    printf ("Sender @ " );
    /* print my MAC address */
    for (i = 0; i < LINKADDR_SIZE; i++)
      printf ("%02x:", linkaddr_node_addr.u8[i]);
    printf (" got %u from %s\n", datum,sender_p);
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
  static char *parent;
  parent = (char * )data;

  printf ("Buzzer process started by %s\n", parent);

  /* This is a variable to store datum sent by other processes */
  static uint8_t datum;

  /* These are variables to store strings for each sending process  */
  static char *lvl_p = "level process";

  /* This variable stores a string pointer */
  static char *sender_p;

  /* We tell the system the application will drive the pin */
  GPIO_SOFTWARE_CONTROL (BUZZER_PORT_BASE, BUZZER_PIN_MASK);

  /* And set as output, starting low */
  GPIO_SET_OUTPUT (BUZZER_PORT_BASE, BUZZER_PIN_MASK);
  GPIO_SET_PIN (BUZZER_PORT_BASE, BUZZER_PIN_MASK);

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
      sender_p = lvl_p;
      if (datum == YELLOW_TRESHOLD)
      {
        printf ("Buzzer off\n");
        GPIO_CLR_PIN (BUZZER_PORT_BASE, BUZZER_PIN_MASK);
      }
      else if (datum == RED_TRESHOLD)
      {
        printf ("Buzzer on\n");
        GPIO_SET_PIN (BUZZER_PORT_BASE, BUZZER_PIN_MASK);
      }
    }
    printf ("Buzzer got %u from %s\n", datum, sender_p);
  }

  /* This is the end of the process, we tell the system we are done.  Even if
   * we won't reach this due to the "while(...)" we need to include it
   */
  PROCESS_END();
}


/*---------------------------------------------------------------------------*/
