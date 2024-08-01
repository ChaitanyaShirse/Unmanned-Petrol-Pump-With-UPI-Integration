#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// OLED and LCD configurations
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change 0x27 to your I2C address if different

// GSM configurations
SoftwareSerial gsmSerial(7, 8); // RX, TX (Ensure RX of GSM is connected to TX of Arduino and vice versa)

// Relay and Motor
const int relayPin = 4;
const float FUEL_RATE_RS_PER_LITER = 100.0; // Fuel rate in Rs per liter
const float PUMP_RATE_LITERS_PER_SECOND = 0.5; // Fuel pump rate in liters per second
const float MIN_AMOUNT = 10.0; // Minimum amount required to start fueling

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
    digitalWrite(relayPin, HIGH); // Relay is off initially

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

    // Wait for user to enter the Rs amount
    float enteredAmount = 0;
    while (!Serial.available()) {
        // Wait for user input
    }
    enteredAmount = Serial.parseFloat();
    Serial.print(F("Amount entered: Rs."));
    Serial.println(enteredAmount);

    // Show the entered amount on the display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Verifying...");
    lcd.setCursor(0, 1);
    lcd.print("Rs amount: ");
    lcd.print(enteredAmount);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Verifying...");
    display.setCursor(10, 0);
    display.print("Rs amount: ");
    display.print(enteredAmount);
    display.display();

    if (enteredAmount >= MIN_AMOUNT) {
        // Await GSM message and verify amount
        float gsmAmount = 0;
        if (verifyPayment(enteredAmount, gsmAmount)) {
            // Calculate fuel quantity and fueling time
            float fuelQuantity = enteredAmount / FUEL_RATE_RS_PER_LITER; // Calculate the amount of fuel in liters
            float fuelingTime = fuelQuantity / PUMP_RATE_LITERS_PER_SECOND; // Calculate fueling time in seconds

            // Turn on the pump and display fueling info
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Fueling...");
            lcd.setCursor(0, 1);
            lcd.print("Amt: Rs.");
            lcd.print(enteredAmount);
            lcd.print(" Q: ");
            lcd.print(fuelQuantity, 2);
            display.clearDisplay();
            display.setCursor(0, 0);
            display.print("Fueling...");
            display.setCursor(10, 0);
            display.print("Amt: Rs.");
            display.print(enteredAmount);
            display.setCursor(10, 64);
            display.print(" Q: ");
            display.print(fuelQuantity, 2);
            display.display();
            Serial.println(F("Fueling..."));
            Serial.print("Amt: Rs.");
            Serial.print(enteredAmount);
            Serial.print(" Q: ");
            Serial.println(fuelQuantity, 2);

            digitalWrite(relayPin, LOW); // Turn on the pump
            delay(fuelingTime * 1000); // Fueling time in milliseconds
            digitalWrite(relayPin, HIGH); // Turn off the pump

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
            display.setCursor(10, 0);
            display.print("Fuel: ");
            display.print(fuelQuantity, 2);
            display.print(" L");
            display.display();
            Serial.println(F("Fueling Complete"));
            Serial.print("Fuel: ");
            Serial.println(fuelQuantity, 2);
            delay(10000); // Show completion message for 10 seconds
        } else {
            // Display error message if GSM amount verification fails
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Verification failed");
            lcd.setCursor(0, 1);
            lcd.print("Retry");
            display.clearDisplay();
            display.setCursor(0, 0);
            display.print("Verification failed");
            display.setCursor(10, 0);
            display.print("Retry");
            display.display();
            Serial.println(F("GSM amount verification failed"));
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

// Verify any incoming GSM message and check if amount matches entered amount
bool verifyPayment(float enteredAmount, float &gsmAmount) {
    unsigned long startTime = millis();
    while (millis() - startTime < 60000) { // Wait up to 60 seconds for payment confirmation
        if (gsmSerial.available()) {
            String message = gsmSerial.readString();
            Serial.println("Received SMS: " + message); // Debugging

            // Parse the amount from the message
            gsmAmount = parseAmountFromMessage(message);
            Serial.print("Extracted amount: ");
            Serial.println(gsmAmount); // Debugging

            // Compare entered amount with extracted GSM amount
            if (enteredAmount == gsmAmount) {
                return true; // Amounts match
            } else {
                Serial.println("Amount mismatch");
                return false; // Amounts do not match
            }
        }
    }
    return false; // No message received within the timeout
}

// Extract amount from GSM message
float parseAmountFromMessage(String message) {
    // Assume the amount is in the format "Amount: Rs.<amount>" in the message
    int startIndex = message.indexOf("Rs.");
    if (startIndex != -1) {
        int endIndex = message.indexOf(' ', startIndex); // Find the end of the amount
        if (endIndex == -1) {
            endIndex = message.length();
        }
        String amountString = message.substring(startIndex + 3, endIndex);
        return amountString.toFloat();
    }
    return 0.0; // Return 0 if the amount cannot be found
}
