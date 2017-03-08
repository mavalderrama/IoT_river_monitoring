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
