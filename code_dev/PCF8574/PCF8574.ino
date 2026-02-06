#include <Wire.h> // Required for I2C communication
#include <PCF8574.h> // Example library, you might use a different one

// Define the I2C address of your PCF8574 (commonly 0x27 or 0x3F)
PCF8574 pcf8574(0x26); 

void setup() {
  Serial.begin(9600);
  Wire.begin(); // Initialize I2C

  pcf8574.begin(); // Initialize the PCF8574

  // Set the pin connected to the button as input with internal pull-up
  pcf8574.pinMode(P0, INPUT); // Assuming button connected to P0
}

void loop() {
  // Read the state of the button
  int buttonState = pcf8574.digitalRead(P0);

  if (buttonState == LOW) { // Button pressed
    Serial.println("Button Pressed!");
    delay(50); // Debounce delay
  }
}
