#include<Arduino.h>

#define CMPSR_PIN 33 // turn compressor on (HIGH) / off (LOW)
#define RV_PIN 26 // set reversing valve to cold (HIGH) / heat (LOW)
#define FAN_PIN 27 // set fan on (HIGH) / off (LOW)
#define EM_PIN 25 // set emergency heat on (HIGH) / off (LOW)

// Declare functions
static void setupControl();
static void updateControl(int current_temp);

int set_temp = 70; // set temperature command in
bool heat_cool = false; // input for cooling (true) / heat (false)
bool fan_in = false; // input for fan on/auto

int cpsr_state_change_countdown = -1; // timer to track compressor state change lockout
const int cpsr_state_delay = 180000; // timer in main loops (three minutes for current set) between compressor, RV, or emheat state changes
// const int cpsr_state_delay = 2; // short timer for troubleshooting

int fan_delay_countdown = -1; // timer to track fan shutdown delay
const int fan_delay = 300000; // timer in main loops (three minutes for current set) between compressor turning off and fan turning off
// const int fan_delay = 2; // short timer for troubleshooting

// Vars for updateControl function
int control_error;
bool cpsr_change_ok;
bool fan_change_ok;
bool compressor_state;
bool RV_state;
bool EM_state;
bool Fan_state;

static void setupControl() {
    pinMode(CMPSR_PIN, OUTPUT);
    pinMode(RV_PIN, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);
    pinMode(EM_PIN, OUTPUT);
}

static void updateControl(int current_temp) {
    control_error = current_temp - set_temp;
    cpsr_change_ok = true;
    fan_change_ok = true;
    compressor_state = (digitalRead(CMPSR_PIN) == HIGH);
    RV_state = (digitalRead(RV_PIN) == HIGH);
    EM_state = (digitalRead(EM_PIN) == HIGH);
    Fan_state = (digitalRead(FAN_PIN) == HIGH);

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
                if (!compressor_state || RV_state || !EM_state) {
                    cpsr_state_change_countdown = cpsr_state_delay;
                }
                digitalWrite(CMPSR_PIN, HIGH);
                digitalWrite(RV_PIN, LOW);
                digitalWrite(EM_PIN, HIGH);
            }
        } else {
            // Normal heat ON condition
            if (cpsr_change_ok) {
                if (!compressor_state || RV_state || EM_state) {
                    cpsr_state_change_countdown = cpsr_state_delay;
                }
                digitalWrite(CMPSR_PIN, HIGH);
                digitalWrite(RV_PIN, LOW);
                digitalWrite(EM_PIN, LOW);
            }
        }
    } else if (control_error > 0) {
        // Normal cool ON condition
        if (cpsr_change_ok) {
            if (!compressor_state || !RV_state || EM_state) {
                cpsr_state_change_countdown = cpsr_state_delay;
            }
            digitalWrite(CMPSR_PIN, HIGH);
            digitalWrite(RV_PIN, HIGH);
            digitalWrite(EM_PIN, LOW);
        }
    } else {
        // Target condition met
        if (cpsr_change_ok) {
            if (compressor_state || RV_state || EM_state) {
                cpsr_state_change_countdown = cpsr_state_delay;
            }
            digitalWrite(CMPSR_PIN, LOW);
            digitalWrite(RV_PIN, LOW);
            digitalWrite(EM_PIN, LOW);
        }
    }

    // Set fan control
    if (fan_change_ok && fan_in) {
        if (!Fan_state) {
            fan_delay_countdown = fan_delay;
        }
        digitalWrite(FAN_PIN, HIGH);
    } else if (fan_change_ok && compressor_state) {
        if (!Fan_state) {
            fan_delay_countdown = fan_delay;
        }
        digitalWrite(FAN_PIN, HIGH);
    } else if (fan_change_ok) {
        if (Fan_state) {
            fan_delay_countdown = fan_delay;
        }
        digitalWrite(FAN_PIN, LOW);
    }
}