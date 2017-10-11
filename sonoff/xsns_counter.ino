/*
  xsns_counter.ino - Counter sensors (water meters, electricity meters etc.) sensor support for Sonoff-Tasmota

  Copyright (C) 2017  Maarten Damen and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*********************************************************************************************\
 * Counter sensors (water meters, electricity meters etc.)
\*********************************************************************************************/

unsigned long pTimeLast[MAX_COUNTERS]; // Last counter time in milli seconds

void counter_update(byte index, int8_t increment)
{
  unsigned long pTime = millis() - pTimeLast[index -1];
  if (pTime > sysCfg.pCounterDebounce) {
    pTimeLast[index -1] = millis();
    if (bitRead(sysCfg.pCounterType, index -1)) {
      rtcMem.pCounter[index -1] = pTime;
    } else {
      rtcMem.pCounter[index -1] += increment;
    }

//    snprintf_P(log_data, sizeof(log_data), PSTR("CNTR: Interrupt %d"), index);
//    addLog(LOG_LEVEL_DEBUG);
  }
}

void counter_update1()
{
  counter_update(1, 1);
}

void counter_update2()
{
  counter_update(2, 1);
}

void counter_update3()
{
  counter_update(3, 1);
}

void counter_update4()
{
  counter_update(4, 1);
}

void counter_savestate()
{
  for (byte i = 0; i < MAX_COUNTERS; i++) {
    if (pin[GPIO_CNTR1 +i] < 99) {
      sysCfg.pCounter[i] = rtcMem.pCounter[i];
    }
  }
}

void counter_init()
{
  typedef void (*function) () ;
  function counter_callbacks[] = { counter_update1, counter_update2, counter_update3, counter_update4 };

  for (byte i = 0; i < MAX_COUNTERS; i++) {
    if (pin[GPIO_CNTR1 +i] < 99) {
      pinMode(pin[GPIO_CNTR1 +i], INPUT_PULLUP);
      attachInterrupt(pin[GPIO_CNTR1 +i], counter_callbacks[i], FALLING);
    }
  }
}

// I tried doing this with interrupts but it was far too noisy. This confuses
// the direction occasionally when moving fast, but usually is correct.
uint8_t last_rotary_clk, last_rotary_dt;
void rotary_check() {
    uint8_t rotary_clk = digitalRead(pin[GPIO_ROTARY_CLK]);
    uint8_t rotary_dt = digitalRead(pin[GPIO_ROTARY_DT]);

    if( rotary_clk != last_rotary_clk || rotary_dt != last_rotary_dt ) {
        // Reaches the new point when both values have become the same
        if( rotary_clk == rotary_dt ) {
            addLog_P(LOG_LEVEL_INFO, rotary_clk != last_rotary_clk ? "Rotor changed down" : "Rotor changed up");
            counter_update( 1, rotary_clk != last_rotary_clk ? -1 : 1 );
#ifdef USE_I2C_LCD
            if( lcd_flg )
                lcd_print("");
#endif
        }

        last_rotary_dt = rotary_dt;
        last_rotary_clk = rotary_clk;
    }
}

void rotary_init()
{
    if (pin[GPIO_ROTARY_CLK] < 99 && pin[GPIO_ROTARY_DT] < 99) {
      rot_flg = 1;
      pinMode(pin[GPIO_ROTARY_CLK], INPUT);
      pinMode(pin[GPIO_ROTARY_DT], INPUT);
      last_rotary_clk = digitalRead(pin[GPIO_ROTARY_CLK]);
      last_rotary_dt = digitalRead(pin[GPIO_ROTARY_DT]);
    }
}

/*********************************************************************************************\
 * Presentation
\*********************************************************************************************/

void counter_mqttPresent(uint8_t* djson)
{
  char stemp[16];

  byte dsxflg = 0;
  for (byte i = 0; i < MAX_COUNTERS; i++) {
    if (pin[GPIO_CNTR1 +i] < 99) {
      if (bitRead(sysCfg.pCounterType, i)) {
        dtostrfd((double)rtcMem.pCounter[i] / 1000, 3, stemp);
      } else {
        dsxflg++;
        dtostrfd(rtcMem.pCounter[i], 0, stemp);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, \"" D_COUNTER "%d\":%s"), mqtt_data, i +1, stemp);
      *djson = 1;
#ifdef USE_DOMOTICZ
      if (1 == dsxflg) {
        domoticz_sensor6(rtcMem.pCounter[i]);
        dsxflg++;
      }
#endif  // USE_DOMOTICZ
    }
  }
}

#ifdef USE_WEBSERVER
const char HTTP_SNS_COUNTER[] PROGMEM =
  "<tr><th>" D_COUNTER "%d</th><td>%s%s</td></tr>";

String counter_webPresent()
{
  String page = "";
  char stemp[16];
  char sensor[80];

  for (byte i = 0; i < MAX_COUNTERS; i++) {
    if (pin[GPIO_CNTR1 +i] < 99) {
      if (bitRead(sysCfg.pCounterType, i)) {
        dtostrfi((double)rtcMem.pCounter[i] / 1000, 3, stemp);
      } else {
        dtostrfi(rtcMem.pCounter[i], 0, stemp);
      }
      snprintf_P(sensor, sizeof(sensor), HTTP_SNS_COUNTER, i+1, stemp, (bitRead(sysCfg.pCounterType, i)) ? " " D_UNIT_SECOND : "");
      page += sensor;
    }
  }
  return page;
}
#endif  // USE_WEBSERVER

