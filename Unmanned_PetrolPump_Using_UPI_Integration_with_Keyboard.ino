#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <Keypad.h>

// OLED and LCD configurations
#define SCREEN_WIDTH 58 // Corrected from 48
#define SCREEN_HEIGHT 8 // Corrected from 8
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change 0x27 to your I2C address if different

// GSM configurations
SoftwareSerial gsmSerial(2, 3); // RX, TX (Ensure RX of GSM is connected to TX of Arduino and vice versa)

// Relay and Motor
const int relayPin = 13;
const float FUEL_RATE_RS_PER_LITER = 100.0; // Fuel rate in Rs per liter
const float PUMP_RATE_LITERS_PER_SECOND = 0.5; // Fuel pump rate in liters per second
const float MIN_AMOUNT = 10.0; // Minimum amount required to start fueling

// Keypad configuration
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {12, 11, 10, 9}; // Connect to the row pinouts of the keypad
byte colPins[COLS] = {8, 7, 6, 5}; // Connect to the column pinouts of the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
    // Initialize Serial for debugging
    Serial.begin(9600);

    // Initialize OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Check OLED I2C address
        Serial.println(F("SSD1306 allocation failed"));
        while (true); // Halt the program
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Enter Rs amount");
    display.display();
    Serial.println(F("Enter Rs amount"));

    // Initialize LCD with columns and rows
    lcd.begin(16, 2); // 16 columns, 2 rows
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Enter Rs amount");

    // Initialize GSM
    gsmSerial.begin(9600);

    // Initialize Relay
    pinMode(relayPin, OUTPUT); 
    digitalWrite(relayPin, HIGH);

    // Show initial message
    delay(2000);
}

void loop() {
    // Enter the Rs amount for petrol
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Enter Rs:");
    display.display();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Rs amount:");
    Serial.println(F("Enter Rs amount:"));

    // Wait for user to enter the Rs amount using keypad
    float amount = readAmountFromKeypad();
    Serial.print(F("Amount entered: Rs."));
    Serial.println(amount);

    // Show the entered amount on the display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Verifying...");
    lcd.setCursor(0, 1);
    lcd.print("Rs amount: ");
    lcd.print(amount);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Rs amount: ");
    display.print(amount);
    display.display();

    if (amount >= MIN_AMOUNT) {
        // Await any GSM message for verification
        if (verifyPayment(amount)) {
            // Calculate fuel quantity and fueling time
            float fuelQuantity = amount / FUEL_RATE_RS_PER_LITER; // Calculate the amount of fuel in liters
            float fuelingTime = fuelQuantity / PUMP_RATE_LITERS_PER_SECOND; // Calculate fueling time in seconds

            // Turn on the pump and display fueling info
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Fueling...");
            lcd.setCursor(0, 1);
            lcd.print("Amt: Rs.");
            lcd.print(amount);
            lcd.print(" Q: ");
            lcd.print(fuelQuantity, 2);
            display.clearDisplay();
            display.setCursor(0, 0);
            display.print("Fueling...");
            display.setCursor(0, 10);
            display.print("Amt: Rs.");
            display.print(amount);
            display.print(" Q: ");
            display.print(fuelQuantity, 2);
            display.display();
            Serial.println(F("Fueling..."));
            Serial.print("Amt: Rs.");
            Serial.print(amount);
            Serial.print(" Q: ");
            Serial.println(fuelQuantity, 2);

            digitalWrite(relayPin, LOW); 
            delay(fuelingTime * 1000); // Fueling time in milliseconds
            digitalWrite(relayPin, HIGH);

            // Display fueling complete with total fuel dispensed
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Fueling Complete");
            lcd.setCursor(0, 1);
            lcd.print("Fuel: ");
            lcd.print(fuelQuantity, 2);
            lcd.print(" L");
            display.clearDisplay();
            display.setCursor(0, 0);
            display.print("Fueling Complete");
            display.setCursor(0, 10);
            display.print("Fuel: ");
            display.print(fuelQuantity, 2);
            display.print(" L");
            display.display();
            Serial.println(F("Fueling Complete"));
            Serial.print("Fuel: ");
            Serial.println(fuelQuantity, 2);
            delay(10000); // Show completion message for 10 seconds
        } else {
            // Display error message if no GSM message is received
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Not verified");
            lcd.setCursor(0, 1);
            lcd.print("Retry");
            display.clearDisplay();
            display.setCursor(0, 0);
            display.print("Not verified");
            display.setCursor(0, 10);
            display.print("Retry");
            display.display();
            Serial.println(F("No GSM message received"));
            delay(5000); // Show error message for 5 seconds
        }
    } else {
        // Display error message if amount is less than the minimum
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Min Amt Rs 10");
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print("Min Amt Rs 10");
        display.display();
        Serial.println(F("Minimum amount is Rs.10"));
        delay(5000); // Show error message for 5 seconds
    }

    delay(10000); // Delay before next cycle
}

// Verify any incoming GSM message and return true if a message with the correct amount is received
bool verifyPayment(float enteredAmount) {
    unsigned long startTime = millis();
    while (millis() - startTime < 60000) { // Wait up to 60 seconds for payment confirmation
        if (gsmSerial.available()) {
            String message = gsmSerial.readString();
            Serial.println("Received SMS: " + message); // Debugging
            if (message.startsWith("INR")) {
                int index = message.indexOf(' ');
                if (index != -1) {
                    String amountStr = message.substring(index + 1);
                    float receivedAmount = amountStr.toFloat();
                    Serial.print("Received Amount: Rs.");
                    Serial.println(receivedAmount);
                    if (receivedAmount == enteredAmount) {
                        return true; // Payment verified
                    }
                }
            }
        }
    }
    return false; // No valid message received within the timeout
}

// Read the amount from the keypad
float readAmountFromKeypad() {
    String input = "";
    char key;
    while (true) {
        key = keypad.getKey();
        if (key) {
            if (key == '#') { // Assume '#' is the enter key
                break;
            } else if (key >= '0' && key <= '9') {
                input += key;
                lcd.setCursor(0, 1);
                lcd.print("Amount: ");
                lcd.print(input);
                display.clearDisplay();
                display.setCursor(0, 10);
                display.print("Amount: ");
                display.print(input);
                display.display();
            }
        }
    }
    return input.toFloat();
}
