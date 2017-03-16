/**
 * \author
 *         5AC1 team
 *          J. Echaiz
 *          A. Hoyos
 *          A. Rudqvist
 *          E. Tamura
 *          M. Valderrama
 */

#ifndef SMAR2C_
#define SMAR2C_
/*---------------------------------------------------------------------------*/

#define NORMAL_LEVEL    2
#define YELLOW_TRESHOLD 6
#define RED_TRESHOLD    8

#define FLOW_SPERIOD    30
#define TEMP_SPERIOD    50

/*
#define BUZZER_PORT_BASE  GPIO_PORT_TO_BASE(GPIO_B_NUM)
#define BUZZER_PIN_MASK   GPIO_PIN_MASK(2)
*/
#define BUZZER_PORT_BASE  GPIO_PORT_TO_BASE(GPIO_A_NUM)
#define BUZZER_PIN_MASK   GPIO_PIN_MASK(5)


/*---------------------------------------------------------------------------*/
#endif /* SMAR2C_ */
