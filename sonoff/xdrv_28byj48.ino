/*
  xdrv_28byj48.ino - 28BYJ-48 4-wire motor support for Sonoff-Tasmota

  Copyright (C) 2017 Mark Zealey

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

#ifdef USE_28BYJ48
#include "xdrv_28byj48.h"

int motor_28byj48_steps_remaining = 0;
motor_28byj48_direction_t motor_28byj48_current_direction = MOTOR_28BYJ48_STOP;
byte motor_28byj48_current_step = 0;

// Can use this to calculate the position of the motor currently based on how
// many times it has gone forwards / back
int total_steps = 0;

void motor_28byj48_run( motor_28byj48_direction_t command, int count )
{
    if( flg_28byj48 ) {
        motor_28byj48_current_direction = command;
        motor_28byj48_steps_remaining = count;
        sleep = 0;
        motor_28byj48_next_step();
    }
}

void motor_28byj48_next_step()
{
    motor_28byj48_steps_remaining--;

    if( motor_28byj48_current_direction == MOTOR_28BYJ48_STOP || motor_28byj48_steps_remaining <= 0 ) {
        motor_28byj48_current_direction = MOTOR_28BYJ48_STOP;
        sleep = sysCfg.sleep;       // resume sleeping normally
    } else {
        if( motor_28byj48_current_direction == MOTOR_28BYJ48_FORWARD ) {
            total_steps++;
            motor_28byj48_current_step++;
        } else if( motor_28byj48_current_direction == MOTOR_28BYJ48_BACK ) {
            total_steps--;
            motor_28byj48_current_step--;
        }

        // Handle wraparounds
        if( motor_28byj48_current_step < 0 )
            motor_28byj48_current_step = 8;
        if( motor_28byj48_current_step > 8 )
            motor_28byj48_current_step = 0;
    }

    // See http://www.bitsbox.co.uk/data/motor/Stepper.pdf for description of the phases of this motor
    int base_wire = motor_28byj48_current_step / 2,
        remainder = motor_28byj48_current_step % 2;

    for (byte i = 0; i < 4; i++) {
        if (pin[GPIO_28BYJ48_PHASE1 +i] < 99) {
            digitalWrite(pin[GPIO_28BYJ48_PHASE1 +i],
                    motor_28byj48_current_direction != MOTOR_28BYJ48_STOP
                    && (
                        ( i == base_wire )
                        || ( i > 0 && i - 1 == base_wire && remainder )
                        || ( i == 0 && motor_28byj48_current_step == 7 )
                    )
            );
        }
    }
}

#endif  // USE_28BYJ48
