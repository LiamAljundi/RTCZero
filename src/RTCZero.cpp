/*
  RTC library for Arduino Zero.
  Copyright (c) 2015 Arduino LLC. All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
 
#include "RTCZero.h"

static bool __time24 = false;

void RTCZero::begin(bool timeRep) 
{
  uint16_t tmp_reg = 0;
  
  if (timeRep)
    __time24 = true; // 24h representation chosen
  
  PM->APBAMASK.reg |= PM_APBAMASK_RTC; // turn on digital interface clock
  config32kOSC();

  // Setup clock GCLK2 with OSC32K divided by 32
  GCLK->GENDIV.reg = GCLK_GENDIV_ID(2)|GCLK_GENDIV_DIV(4);
  while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY)
    ;
  GCLK->GENCTRL.reg = (GCLK_GENCTRL_GENEN | GCLK_GENCTRL_SRC_XOSC32K | GCLK_GENCTRL_ID(2) | GCLK_GENCTRL_DIVSEL );
  while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY)
    ;
  GCLK->CLKCTRL.reg = (uint32_t)((GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2 | (RTC_GCLK_ID << GCLK_CLKCTRL_ID_Pos)));
  while (GCLK->STATUS.bit.SYNCBUSY)
    ;

  RTCdisable();

  RTCreset();

  tmp_reg |= RTC_MODE2_CTRL_MODE_CLOCK; // set clock operating mode
  tmp_reg |= RTC_MODE2_CTRL_PRESCALER_DIV1024; // set prescaler to 1024 for MODE2
  tmp_reg &= ~RTC_MODE2_CTRL_MATCHCLR; // disable clear on match
  
  if (timeRep)
    tmp_reg |= RTC_MODE2_CTRL_CLKREP; // 24h time representation
  else
    tmp_reg &= ~RTC_MODE2_CTRL_CLKREP; // 12h time representation

  RTC->MODE2.READREQ.reg &= ~RTC_READREQ_RCONT; // disable continuously mode

  RTC->MODE2.CTRL.reg = tmp_reg;
  while (RTCisSyncing())
    ;

  NVIC_EnableIRQ(RTC_IRQn); // enable RTC interrupt 
  NVIC_SetPriority(RTC_IRQn, 0x00);

  RTC->MODE2.INTENSET.reg &= ~RTC_MODE2_INTENSET_ALARM0; // disable alarm interrupt by default
  
  while (RTCisSyncing())
    ;

  RTCenable();
  RTCresetRemove();
}

void RTC_Handler(void)
{
  RTC->MODE2.INTFLAG.reg = RTC_MODE2_INTFLAG_ALARM0; // must clear flag at end
}

/*
 * Get Functions
 */

uint8_t RTCZero::getSeconds()
{
  return RTC->MODE2.CLOCK.bit.SECOND;
}

uint8_t RTCZero::getMinutes()
{
  return RTC->MODE2.CLOCK.bit.MINUTE;
}

uint8_t RTCZero::getHours()
{
  return RTC->MODE2.CLOCK.bit.HOUR;
}

uint8_t RTCZero::getDay()
{
  return RTC->MODE2.CLOCK.bit.DAY;
}

uint8_t RTCZero::getMonth()
{
  return RTC->MODE2.CLOCK.bit.MONTH;
}

uint8_t RTCZero::getYear()
{
  return RTC->MODE2.CLOCK.bit.YEAR;
}

/*
 * Set Functions
 */

void RTCZero::setSeconds(uint8_t seconds)
{
  RTC->MODE2.CLOCK.bit.SECOND = seconds;
  while (RTCisSyncing())
    ;
}

void RTCZero::setMinutes(uint8_t minutes)
{
  RTC->MODE2.CLOCK.bit.MINUTE = minutes;
  while (RTCisSyncing())
    ;
}

void RTCZero::setHours(uint8_t hours)
{
  if ((__time24) || (hours < 13)) {
    RTC->MODE2.CLOCK.bit.HOUR = hours;
  } else {
    RTC->MODE2.CLOCK.bit.HOUR = hours - 12;
  }
  while (RTCisSyncing())
    ;
}

void RTCZero::setTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
  setSeconds(seconds);
  setMinutes(minutes);
  setHours(hours);
}

void RTCZero::setDay(uint8_t day)
{
  RTC->MODE2.CLOCK.bit.DAY = day;
  while (RTCisSyncing())
    ;
}

void RTCZero::setMonth(uint8_t month)
{
  RTC->MODE2.CLOCK.bit.MONTH = month;
  while (RTCisSyncing())
    ;
}

void RTCZero::setYear(uint8_t year)
{
  RTC->MODE2.CLOCK.bit.YEAR = year;
  while (RTCisSyncing())
    ;
}

void RTCZero::setDate(uint8_t day, uint8_t month, uint8_t year)
{
  setDay(day);
  setMonth(month);
  setYear(year);
}

/*
 * Private Utility Functions
 */

/* Configure the 32768Hz Oscillator */
void RTCZero::config32kOSC() 
{
  SYSCTRL->XOSC32K.reg = SYSCTRL_XOSC32K_ONDEMAND |
                         SYSCTRL_XOSC32K_RUNSTDBY |
                         SYSCTRL_XOSC32K_EN32K |
                         SYSCTRL_XOSC32K_XTALEN |
                         SYSCTRL_XOSC32K_STARTUP(6) |
                         SYSCTRL_XOSC32K_ENABLE;
}

/* Wait for sync in write operations */
bool RTCZero::RTCisSyncing()
{
  return (RTC->MODE2.STATUS.bit.SYNCBUSY);
}

void RTCZero::RTCdisable()
{
  RTC->MODE2.CTRL.reg &= ~RTC_MODE2_CTRL_ENABLE; // disable RTC
  while (RTCisSyncing())
    ;
}

void RTCZero::RTCenable()
{
  RTC->MODE2.CTRL.reg |= RTC_MODE2_CTRL_ENABLE; // enable RTC
  while (RTCisSyncing())
    ;
}

void RTCZero::RTCreset()
{
  RTC->MODE2.CTRL.reg |= RTC_MODE2_CTRL_SWRST; // software reset
  while (RTCisSyncing())
    ;
}

void RTCZero::RTCresetRemove()
{
  RTC->MODE2.CTRL.reg &= ~RTC_MODE2_CTRL_SWRST; // software reset remove
  while (RTCisSyncing())
    ;
}

