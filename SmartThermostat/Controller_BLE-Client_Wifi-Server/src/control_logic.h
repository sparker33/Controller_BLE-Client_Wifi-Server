#include<Arduino.h>

int set_temp = 0; // set temperature command in
bool heat_cool = false; // input for cooling (true) / heat (false)
bool fan_in = false; // input for fan on/auto

bool compressor_cmd = false; // turn compressor on (true) / off (false)
bool RV_cmd = false; // set reversing valve to cold (true) / heat (false)
bool fan_cmd = false; // set fan on (true) / off (false)
bool emheat_cmd = false; // set emergency heat on (true) / off (false)

int cpsr_state_change_countdown = -1; // timer to track compressor state change lockout
const int cpsr_state_delay = 180000; // three minute timer between compressor, RV, or emheat state changes

int fan_delay_countdown = -1; // timer to track fan shutdown delay
const int fan_delay = 300000; // five minute timer between compressor turning off and fan turning off

static void updateControlStates(int current_temp) {
    int control_error = current_temp - set_temp;
    bool cpsr_change_ok = true;
    bool fan_change_ok = true;

    if (cpsr_state_change_countdown > 0) {
        cpsr_state_change_countdown = cpsr_state_change_countdown - 1;
        cpsr_change_ok = false;
    }
    if (fan_delay_countdown > 0) {
        fan_delay_countdown = fan_delay_countdown - 1;
        fan_change_ok = false;
    }

    if (control_error < 0) {
        // Heat conditions
        if (control_error < -3) {
            // Emergency heat ON condition
            if (cpsr_change_ok) {
                if (!compressor_cmd || RV_cmd || !emheat_cmd) {
                    cpsr_state_change_countdown = cpsr_state_delay;
                }
                compressor_cmd = true;
                RV_cmd = false;
                emheat_cmd = true;
            }
        } else {
            // Normal heat ON condition
            if (cpsr_change_ok) {
                if (!compressor_cmd || RV_cmd || emheat_cmd) {
                    cpsr_state_change_countdown = cpsr_state_delay;
                }
                compressor_cmd = true;
                RV_cmd = false;
                emheat_cmd = false;
            }
        }
    } else if (control_error > 0) {
        // Normal cool ON condition
        if (cpsr_change_ok) {
            if (!compressor_cmd || !RV_cmd || emheat_cmd) {
                cpsr_state_change_countdown = cpsr_state_delay;
            }
            compressor_cmd = true;
            RV_cmd = true;
            emheat_cmd = false;
        }
    } else {
        // Target condition met
        if (cpsr_change_ok) {
            if (compressor_cmd || RV_cmd || emheat_cmd) {
                cpsr_state_change_countdown = cpsr_state_delay;
            }
            compressor_cmd = false;
            RV_cmd = false;
            emheat_cmd = false;
        }
    }

    // Set fan control
    if (fan_change_ok && fan_in) {
        if (!fan_cmd) {
            fan_delay_countdown = fan_delay;
        }
        fan_cmd = true;
    } else if (fan_change_ok && compressor_cmd) {
        if (!fan_cmd) {
            fan_delay_countdown = fan_delay;
        }
        fan_cmd = true;
    } else if (fan_change_ok) {
        if (fan_cmd) {
            fan_delay_countdown = fan_delay;
        }
        fan_cmd = false;
    }
}