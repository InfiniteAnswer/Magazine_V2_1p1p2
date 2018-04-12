// Arduino MEGA 2560
// Victor Samper
// sampervictor@gmail.com
// 10 April 2018
// V. 1.1.0

// Include the Servo library 
#include <Servo.h> 


// Declare pins
// servo pins for SERVO0 - SERVO7
int SERVO_PINS[] = { 2, 3, 4, 5, 6, 7, 8, 9
};

// microswitch pins for CARTRIDGE0 - CARTRIDGE7
int CARTRIDGE_uSW_PIN[] = { 24, 25, 26, 27, 20, 21, 22, 23
};

// push button pin
int BUTTON_PIN = 28;

// chute optical sensor pins for CHUTE0 - CHUTE7 + DETECTOR CHUTE (A8)
int CHUTE_SENS_PIN[] = { A7, A6, A5, A4, A3, A2, A1, A0, A8
};

// LED indicator pins for CARTRIDGE0 - CARTRIDGE7
int INDICATOR_PIN[] = { 33, 34, 35, 36, 37, 38, 39, 40
};

// LED indicator pin for DETECTOR_CHUTE
int DETECTOR_CHUTE_LED_PIN = 32;

// LED indicator pins for communication mode
int COMMS_LED_PIN = 29;
int COMMS_TX_LED_PIN = 30;
int COMMS_RX_LED_PIN = 31;

// LED pins for ambient setting indicators
int AMBIENT_WITHOUT_CHUTE_PIN = 41; // LED ambient without chute level indicator
int AMBIENT_WITH_CHUTE_PIN = 43; // LED ambient with chute level indicator
int TILE_REFLECTION_PIN = 42; // LED for tile reflection level indicator

							  // LED pins for status indicators
int READY_LED_PIN = 47;
int FAIL_LED_PIN = 48;
int ERROR_LED_PIN = 49;

// CONNECTIONS TO MAIN CONTROLLER
int READY_PIN = 52; // to controller AND to indicator
int FAIL_PIN = 51; // to controller AND to indicator
int ERROR_PIN = 50; // to controller AND to indicator

					// USART TX RX
int TX_PIN = 1; // transmit
int RX_PIN = 0; // receive


				// Declare constants
int SERVO_REST = 15;
int SERVO_PUSH = 165;
int SENSOR_TRIGGER_UPPER_DELTA = 20;
int SENSOR_TRIGGER_LOWER_DELTA = 10;
unsigned long RESET_PRESET_TIME = 3000;
boolean ENABLE_SERIAL_MONITOR = false;


// Declare variables
Servo servo0;
Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;
Servo servo5;
Servo servo6;
Servo servo7;
Servo servoX[] = { servo0, servo1, servo2, servo3, servo4, servo5, servo6, servo7 };
long average_optical_SENS = 0;
unsigned long time_switch_released;
int comm = LOW;
int tx = LOW;
int rx = LOW;
int ready = LOW;
int fail = LOW;
int error = LOW;
int ambient_without_chute = LOW;
int ambient_with_chute = LOW;
int tile_reflection = LOW;
int indicator[8] = { LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW };
int detector_chute = LOW;
int detector_chute_sens_ambient = 0;
int detector_chute_sens_close = 0;
int chute_sens_ambient[9];
int chute_sens_empty[9];
int chute_sens_full[8];
int cartridge_present[8];
int detector_level;
unsigned long timeout = 5000;
unsigned long time_start;
int mode = 0; // 0=setup; 1=operation; 2=comms; 3=error


			  // Update cartridges present
void update_cartridge_state() {
	for (int i = 0; i<8; i++) {
		cartridge_present[i] = digitalRead(CARTRIDGE_uSW_PIN[i]);
	}
}


// Update all LED and output pin states
void update_LEDs() {
	digitalWrite(COMMS_LED_PIN, !comm);
	digitalWrite(COMMS_TX_LED_PIN, !tx);
	digitalWrite(COMMS_RX_LED_PIN, !rx);
	digitalWrite(AMBIENT_WITHOUT_CHUTE_PIN, !ambient_without_chute);
	digitalWrite(AMBIENT_WITH_CHUTE_PIN, !ambient_with_chute);
	digitalWrite(TILE_REFLECTION_PIN, !tile_reflection);
	digitalWrite(READY_LED_PIN, !ready);
	digitalWrite(READY_PIN, ready);
	digitalWrite(FAIL_LED_PIN, !fail);
	digitalWrite(FAIL_PIN, fail);
	digitalWrite(ERROR_LED_PIN, !error);
	digitalWrite(ERROR_PIN, error);
	digitalWrite(DETECTOR_CHUTE_LED_PIN, detector_chute);
	for (int i = 0; i<8; i++) {
		digitalWrite(INDICATOR_PIN[I], indicator[i]);
	}
}


// Calculate time passed in ms, including 40 day (32 bit unsigned long) overflow
unsigned long time_passed(unsigned long start_time) {
	unsigned long elapsed_time;
	unsigned long current_time;
	current_time = millis();
	if (current_time<start_time) {
		start_time -= pow(2, 31);
		current_time += pow(2, 31);
	}
	elapsed_time = current_time - start_time;
	return elapsed_time;
}


// Calculate flashing state based on period and start time
int flash_state(unsigned long period, unsigned long start_time) {
	unsigned long elapsed_time = time_passed(start_time);
	unsigned long = elapsed_time / period;
	int state;
	if (elapsed_time % 2 == 0) {
		state = LOW;
	}
	else {
		state = HIGH;
	}
	return state;
}


// Wait for button press, flash LEDs to indicate waiting, return button press duration
unsigned long button_press(boolean flash) {
	int timing[6][4] = {
		{ 100, LOW, HIGH, HIGH },
	{ 200, HIGH, LOW, HIGH },
	{ 300, HIGH, HIGH, LOW },
	{ 500, HIGH, HIGH, HIGH },
	{ 700, HIGH, HIGH, LOW },
	{ 900, HIGH, HIGH, HIGH }
	};
	int time_start = millis();
	int timing_index = 0;
	int button_state = digitalRead(BUTTON_PIN);
	while (button_state == HIGH) {
		if (flash) {
			digitalWrite(READY_LED_PIN, timing[timing_index][1]);
			digitalWrite(FAIL_LED_PIN, timing[timing_index][2]);
			digitalWrite(ERROR_LED_PIN, timing[timing_index][3]);
			if (time_passed(time_start) > timing[timing_index][0]) {
				timing_index++;
			}
			if (timing_index == 6) {
				timing_index = 0;
				time_start = millis();
			}
		}
		button_state = digitalRead(BUTTON_PIN);
		delay(10);
	}
	while (button_state == LOW) {
		button_state = digitalRead(BUTTON_PIN);
		delay(10);
	}
	update_LEDs();
	return time_passed(time_start);
}


// Setup optical sensor average levels for 3 conditions
void setup_ambient_levels() {
	unsigned long start_time = millis();
	unsigned long PERIOD = 200;
	int SAMPLE_TIME = 2000;
	int number_measurements = 0;
	unsigned long running_total;
	unsigned long measurement_time;
	int LED_state = HIGH;

	// Communicate and display state
	error = LOW;
	fail = LOW;
	ready = LOW;
	update_LEDs();

	// Flash READY/FAIL/ERROR LEDs to indicate waiting for button press
	// Send (TRUE) to button_press() to select this mode
	button_press(true);

	// indicate that measuring the ambient without chute by flashing the LED
	// cycle through the 9 measurements, recording signal for 2 seconds each time
	// average and store
	running_total = 0;
	for (int i = 0; i < 9; i++) {
		measurement_time = millis();
		while (time_passed(measurement_time) < SAMPLE_TIME) {
			running_total += analogRead(CHUTE_SENS_PIN[i]);
			number_measurements++;
			digitalWrite(AMBIENT_WITHOUT_CHUTE_PIN, flash_state(PERIOD, start_time));
			delay(50);
		}
		chute_sens_ambient[i] = running_total / number_measurements;
	}

	// indicate that ambient without chute measurement is complete with solid LED
	ambient_without_chute = HIGH;
	digitalWrite(AMBIENT_WITHOUT_CHUTE_PIN, ambient_without_chute);

	// wait for button press before advancing
	button_press(true);

	// indicate that measuring the ambient with chute by flashing the LED
	running_total = 0;
	for (int i = 0; i < 9; i++) {
		measurement_time = millis();
		while (time_passed(measurement_time) < sample_time) {
			running_total += analogRead(CHUTE_SENS_PIN[i]);
			number_measurements++;
			digitalWrite(AMBIENT_WITH_CHUTE_PIN, flash_state(PERIOD, start_time));
			delay(50);
		}
		chute_sens_empty[i] = running_total / number_measurements;
	}

	// indicate that ambient with chute measurement is complete with solid LED
	ambient_with_chute = HIGH;
	digitalWrite(AMBIENT_WITH_CHUTE_PIN, ambient_with_chute);

	// wait for button press before advancing
	button_press(true);

	// indicate that measuring the reflected tile level by flashing the LED
	// cycle through the 8 measurements instead of 9 for previous 2 measurements
	running_total = 0;
	for (int i = 0; i < 8; i++) {
		measurement_time = millis();
		while (time_passed(measurement_time) < sample_time) {
			running_total += analogRead(CHUTE_SENS_PIN[i]);
			number_measurements++;
			digitalWrite(TILE_REFLECTION_PIN, flash_state(PERIOD, start_time));
			delay(50);
		}
		chute_sens_full[i] = running_total / number_measurements;
	}

	// indicate that reflected tile level measurement is complete with solid LED
	tile_reflection = HIGH;
	digitalWrite(TILE_REFLECTION_PIN, tile_reflection);

	// wait for button press before advancing
	button_press(true);

	error = LOW;
	fail = LOW;
	ready = HIGH;
	update_LEDs();

	// exit averaging mode
}


void communicate_status() {

}


void button_pressed() {

}


void short_press() {

}


void long_press() {

}


void chute_detected() {

}


unsigned long time_passed() {

}


void update_cartridge_status() {

}


void setup() {
	// put your setup code here, to run once:

}

void loop() {
	// put your main code here, to run repeatedly:
	Serial.print("test");
	Serial.print("test2");
}
