// Library imports

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;
#include <TouchScreen.h>
#include "RTClib.h"
#include "SHT31.h"
#include "Wire.h"
#include "HX711.h"
#include "SPI.h"
#include "SD.h"
#include "Adafruit_I2CDevice.h"


#define MINPRESSURE 200
#define MAXPRESSURE 1000

// Pin assignments
#define SHT31_ADDRESS      0x45 // SHT sensor address
#define RELAY_PIN_RESI           23  // Relay pin for the heating elements
#define RELAY_PIN_VENTI           22 // Relay pin for the fan
#define HUMIDIFICATEUR_PIN  37 // Humidifier pin
#define LOADCELL_DOUT_PIN    42 // HX711 data output (DT)
#define LOADCELL_SCK_PIN    40  // HX711 clock pin (SCK)
const int chipSelect = 53; // SD card chip-select pin

// Color definitions
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF


// Sensor and peripheral objects
SHT31 sht(SHT31_ADDRESS, &Wire);  // SHT31 sensor
HX711 scale;  // Load cell amplifier
RTC_DS1307 clock;
File myFile;

// Touchscreen pins
const int XP = 8, XM = A2, YP = A3, YM = 9;
// Touchscreen calibration limits
const int TS_LEFT = 946, TS_RT = 80, TS_TOP = 68, TS_BOT = 890;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// Button definitions
Adafruit_GFX_Button blue_btn, red_btn, back_btn, clear_btn, dot_btn,tare_btn;
Adafruit_GFX_Button num_btns[12];

// Global variables
int pixel_x, pixel_y;

// Available screen states
enum ScreenState
{
    MAIN_MENU,
    HUMIDITY_SCREEN,
    TEMPERATURE_SCREEN,
    NUMPAD_SCREEN
};
ScreenState screenState = MAIN_MENU;

// Setpoints
float humiditySetPoint = 0.0;  
float temperatureSetPoint = 0.0;
char numpadInput[6] = "";
int numpadIndex = 0;
ScreenState previousScreenState;

// Timing variables
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 5000; // Measurement update interval in milliseconds (5 seconds)

// Function prototypes

// Touchscreen handling
bool Touch_getXY(void);
void handleMainMenuButtons(bool down);
void handleNumpadButtons(bool down);
void handleNumpadPress(int index);

// Initialization
void initSerial();
void initI2C();
bool initSDCard();
bool initTFT();
bool initRTC();
bool initSHT31();
bool initLoadCell();
void RelaisPins();
bool initializeComponents();

// Display and interface
void displayError(const char* message);
void WelcomeMessage();
void drawMainMenu();
void drawHumidityScreen();
void drawTemperatureScreen();
void drawNumpadScreen(ScreenState currentState);

// Measurement, regulation, and data logging
void SDWriteHeader();
void SDTime();
float ReadTemperature();
float ReadHumidity();
float ReadMasse();
void ControlVentilator(float humidity, float temperature);
void ReguTemperature(float temperature, float humidity);
void ReguHumidity(float humidity, float temperature);
void ScreenTemperature(float temperature);
void ScreenHumidity(float humidity);
void ScreenMasse(float masse);
void SDTemperature(float temperature);
void SDHumidity(float humidity);
void SDMasse(float masse);

// Read the touch coordinates from the touchscreen
bool Touch_getXY(void)
{
    TSPoint p = ts.getPoint();
    pinMode(YP, OUTPUT); // Restore the shared pins
    pinMode(XM, OUTPUT); // because these pins are shared with TFT control
    bool pressed = (p.z > MINPRESSURE && p.z < MAXPRESSURE);
    if (pressed)
    {
        // Adjust coordinates according to screen rotation
        switch (tft.getRotation())
        {
        case 0:
            pixel_x = map(p.x, TS_LEFT, TS_RT, 0, tft.width());
            pixel_y = map(p.y, TS_TOP, TS_BOT, 0, tft.height());
            break;
        case 1:
            pixel_x = map(p.y, TS_TOP, TS_BOT, 0, tft.width());
            pixel_y = map(p.x, TS_RT, TS_LEFT, 0, tft.height());
            break;
        case 2:
            pixel_x = map(p.x, TS_RT, TS_LEFT, 0, tft.width());
            pixel_y = map(p.y, TS_BOT, TS_TOP, 0, tft.height());
            break;
        case 3:
            pixel_x = map(p.y, TS_BOT, TS_TOP, 0, tft.width());
                        pixel_y = map(p.x, TS_LEFT, TS_RT, 0, tft.height());
            break;
        }
    }
    return pressed;
}

// Initialize the serial port
void initSerial() {
    Serial.begin(9600);
    while (!Serial); // Wait for the serial connection
}

// Initialize the I2C bus
void initI2C() {
    Wire.begin();
}

// Initialize the SD card
bool initSDCard() {
    Serial.print("Initializing SD card...");
    if (SD.begin(chipSelect)) {
        Serial.println("Initialization successful.");
        myFile = SD.open("DATA.txt", FILE_WRITE);
        if (myFile) {
            Serial.println("File opened successfully.");
            myFile.println("Date,Time,Temperature,Humidity,Mass");
            myFile.close();
            return true;
        } else {
            Serial.println("Error opening file.");
            return false;
        }
    } else {
        Serial.println("Initialization failed!");
        tft.print("SD card initialization failed.");
        return false;
    }
}


// Initialize the TFT display
bool initTFT() {
    uint16_t ID = tft.readID();
    tft.begin(ID);
    tft.setRotation(3); // Landscape orientation
    return true;
}

// Initialize the real-time clock
bool initRTC() {
    if (!clock.begin()) {
        Serial.println("RTC not detected.");
        return false;
    } else {
        //clock.adjust(DateTime(F(__DATE__), F(__TIME__)));
        if (!clock.isrunning()) {
            Serial.println("RTC is not running!");
            return false;
        }
    }
    return true;
}

// Initialize the SHT31 sensor
bool initSHT31() {
    return sht.begin();
}

// Initialize the load cell
bool initLoadCell() {
    Serial.println("Initializing load cell...");
    
    // Initialize the load cell
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    
    // Set the calibration factor
    scale.set_scale(1062); // Make sure this factor is correct for your setup
    
    // Tare the load cell
    scale.tare(); 

    // Check whether the load cell is responding correctly
    delay(1000); // Wait briefly for the readings to stabilize
    long reading = scale.get_units(10); // Read multiple samples for improved precision

    if (abs(reading) < 1000) { // Check whether the reading is within a reasonable range
        Serial.println("Load cell initialized successfully.");
        return true;
    } else {
        Serial.println("Load cell initialization error: invalid reading.");
        return false;
    }
}

// Initialize relay pins
void RelaisPins() {
    pinMode(RELAY_PIN_RESI, OUTPUT);
    pinMode(RELAY_PIN_VENTI, OUTPUT);
    pinMode(HUMIDIFICATEUR_PIN, OUTPUT);
    

    digitalWrite(RELAY_PIN_RESI, LOW);
    digitalWrite(RELAY_PIN_VENTI, LOW);
    digitalWrite(HUMIDIFICATEUR_PIN, LOW);
    
}

// Display an error message
void displayError(const char* message) {
    tft.fillScreen(WHITE); // Clear the screen
    tft.setTextColor(RED); // Couleur du texte en rouge
    tft.setTextSize(2); // Text size

    // Calcul des dimensions du texte
    int16_t x, y;
    uint16_t w, h;
    
    // Calculate the dimensions of the error message
    tft.getTextBounds(message, 0, 0, &x, &y, &w, &h);
    int16_t xPos = (tft.width() - w) / 2; // Centered horizontal position
    int16_t yPos = (tft.height() - h) / 2 - h / 2; // Centered vertical position

    // Display the error message
    tft.setCursor(xPos, yPos);
    tft.println(message);

    // Display the restart message
    const char* restartMessage = "Restarting in 5 seconds...";
    tft.getTextBounds(restartMessage, 0, 0, &x, &y, &w, &h);
    int16_t xPosRestart = (tft.width() - w) / 2; // Centered horizontal position
    int16_t yPosRestart = (tft.height() - h) / 2 + h; // Vertical position just below the error message

    // Display the restart message
    tft.setCursor(xPosRestart, yPosRestart);
    tft.println(restartMessage);

    delay(5000); // Attendre avant de réessayer
}


// Check component initialization
bool initializeComponents() {

    if (!initSDCard()) {
        displayError("SD Card Error");
        return false; // Initialization failed
    }
    if (!initTFT()) {
        displayError("TFT Display Error");
        return false; // Initialization failed
    }

    if (!initRTC()) {
        displayError("RTC Error");
        return false; // Initialization failed
    }

    if (!initSHT31()) {
        displayError("SHT31 Sensor Error");
        return false; // Initialization failed
    }

    if (!initLoadCell()) {
        displayError("Load Cell Error");
        return false; // Initialization failed
    }

    return true; // All components initialized successfully
}


// Display the welcome message
void WelcomeMessage() {
    // Set the background and font
    tft.setTextSize(2); // Text size
    tft.fillScreen(WHITE); // Fond blanc
    tft.setTextColor(BLACK); // Couleur du texte

    // Calculate text positions for centering
    int16_t x1, y1;
    uint16_t w, h;

    // Center "HELLO"
    tft.getTextBounds("HELLO!", 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((tft.width() - w) / 2, 50);
    tft.println("HELLO!");

    // Center "DEAR"
    tft.getTextBounds("DEAR", 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((tft.width() - w) / 2, 100);
    tft.println("DEAR");
    
    // Center "USER"
    tft.getTextBounds("USER", 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((tft.width() - w) / 2, 150);
    tft.println("USER");

    delay(5000);
    tft.fillScreen(WHITE); // Clear the screen before displaying temperature and humidity values
    tft.setTextColor(BLACK);
}

// Draw the main menu
void drawMainMenu()
{
    tft.fillScreen(WHITE);
    blue_btn.initButton(&tft, 250, 110, 70, 30, WHITE, BLUE, BLACK, (char *)"Setpoint", 1);
    red_btn.initButton(&tft, 250, 60, 70, 30, WHITE, RED, BLACK, (char *)"Setpoint", 1);
    tare_btn.initButton(&tft, 250, 160, 70, 30, BLACK, GREEN, BLACK, (char *)"Tare", 1);
    blue_btn.drawButton();
    red_btn.drawButton();
    tare_btn.drawButton();
}


// Draw the humidity setpoint screen
void drawHumidityScreen()
{
    tft.fillScreen(WHITE);
    drawNumpadScreen(HUMIDITY_SCREEN);
}

// Draw the temperature setpoint screen
void drawTemperatureScreen()

{
    tft.fillScreen(WHITE);
    drawNumpadScreen(TEMPERATURE_SCREEN);
}

// Draw the numeric keypad
void drawNumpadScreen(ScreenState currentState)
{
    tft.fillScreen(WHITE);

    // Draw the numeric buttons
    const int btnSize = 40; // button size
    const int btnSpacing = 5; // spacing between buttons
    int btnX, btnY;

    // Draw buttons 1 to 9
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            int btnIndex = i * 3 + j + 1;
            char label[2];
            sprintf(label, "%d", btnIndex);
            btnX = 20 + j * (btnSize + btnSpacing);
            btnY = 30 + i * (btnSize + btnSpacing);
            num_btns[btnIndex].initButton(&tft, btnX, btnY, btnSize, btnSize, WHITE, BLACK, WHITE, label, 2);
            num_btns[btnIndex].drawButton();
        }
    }

    // Bouton "0"
    num_btns[0].initButton(&tft, 20 + btnSize + btnSpacing, 30 + 3 * (btnSize + btnSpacing), btnSize, btnSize, WHITE, BLACK, WHITE, (char *)"0", 2);
    num_btns[0].drawButton();

    // Clear button
    clear_btn.initButton(&tft, 20, 30 + 3 * (btnSize + btnSpacing), btnSize, btnSize, WHITE, BLACK, WHITE, (char *)"<-", 2);
    clear_btn.drawButton();

    // OK button
    num_btns[10].initButton(&tft, 20 + 2 * (btnSize + btnSpacing), 30 + 3 * (btnSize + btnSpacing), btnSize, btnSize, WHITE, BLACK, WHITE, (char *)"OK", 2);
    num_btns[10].drawButton();

    // Decimal-point button
    int dotBtnX = 20 + btnSize + btnSpacing; // Same X position as the "0" button
    int dotBtnY = 30 + 4 * (btnSize + btnSpacing); // Place below the "0" button
    dot_btn.initButton(&tft, dotBtnX, dotBtnY, btnSize, btnSize, WHITE, BLACK, WHITE, (char *)".", 2);
    dot_btn.drawButton();

    // Draw the current input display area
    tft.fillRect(140, 60, 60, 30, BLACK);
    tft.drawRect(140, 60, 60, 30, BLACK);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(145, 65);
    tft.print(numpadInput);

    // Display the temperature setpoint
    if (currentState == TEMPERATURE_SCREEN)
    {
        tft.setTextSize(2);
        tft.setTextColor(BLACK);
        tft.setCursor(140, 100);
        tft.print("Temperature");
        tft.setCursor(160, 130);
        tft.print("Setpoint:");
        tft.setCursor(180, 160);
        char tempString[6];
        dtostrf(temperatureSetPoint, 4, 1, tempString); // Convert the temperature setpoint to a string with four characters and one decimal place
        tft.print(tempString);
        tft.print(" C");
    }

    // Display the humidity setpoint
    if (currentState == HUMIDITY_SCREEN)
    {
        tft.setTextSize(2);
        tft.setTextColor(BLACK);
        tft.setCursor(140, 100);
        tft.print("Humidity");
        tft.setCursor(155, 130);
        tft.print("Setpoint:");
        tft.setCursor(180, 160);
        char humString[6];
        dtostrf(humiditySetPoint, 4, 1, humString); // Convert the humidity setpoint to a string with four characters and one decimal place
        tft.print(humString);
        tft.print(" %");
    }

    // Back button 
    back_btn.initButton(&tft, 280, 220, 40, 30, WHITE, BLACK, WHITE, (char *)"Back", 1);
    back_btn.drawButton();
}

// Handle numeric keypad button presses
void handleNumpadPress(int index)
{
    if (index < 10)
    {
        // Append a digit to the input
        if (numpadIndex < 4)
        {
            numpadInput[numpadIndex++] = '0' + index;
            numpadInput[numpadIndex] = '\0';
            drawNumpadScreen(screenState);
        }
    }
    else
    {
        // OK button pressed
        float value = atof(numpadInput);
        if (screenState == HUMIDITY_SCREEN)
        {
            if (value >= 0 && value <= 100)
            {
                humiditySetPoint = value;
            }
            else
            {
                // Display an error message if the value is out of range
                tft.setTextSize(1);
                tft.setTextColor(RED);
                tft.setCursor(140, 40);
                tft.print("WARNING: MAXIMUM VALUE");
                tft.setCursor(140, 50);
                tft.print("(100 %) EXCEEDED");
                delay(3000); // Wait three seconds before clearing the message
                tft.fillRect(140, 50, 320, 20, BLACK); // Clear the message after three seconds
            }
        }
        else if (screenState == TEMPERATURE_SCREEN)
        {
            if (value >= 0 && value <= 85)
            {
                temperatureSetPoint = value;
            }
            else
            {
                // Display an error message if the value is out of range
                tft.setTextSize(1);
                tft.setTextColor(RED);
                tft.setCursor(140, 40);
                tft.print("WARNING: MAXIMUM VALUE");
                tft.setCursor(140, 50);
                tft.print("(85 C) EXCEEDED");
                delay(3000); // Wait three seconds before clearing the message
                tft.fillRect(140, 50, 320, 20, BLACK); // Clear the message after three seconds
            }
        }
        // Reset the keypad input
        memset(numpadInput, 0, sizeof(numpadInput));
        numpadIndex = 0;
        drawNumpadScreen(screenState);
    }
    
}

// Handle actions in the main menu
void handleMainMenuButtons(bool down) {
    blue_btn.press(down && blue_btn.contains(pixel_x, pixel_y));
    red_btn.press(down && red_btn.contains(pixel_x, pixel_y));
    tare_btn.press(down && tare_btn.contains(pixel_x, pixel_y));

    // If the blue setpoint button is pressed
    if (blue_btn.justPressed()) {
        blue_btn.drawButton(true);
        red_btn.drawButton();
        tare_btn.drawButton();
        screenState = HUMIDITY_SCREEN;
        drawHumidityScreen();
    }
    // If the red setpoint button is pressed
    else if (red_btn.justPressed()) {
        red_btn.drawButton(true);
        blue_btn.drawButton();
        tare_btn.drawButton();
        screenState = TEMPERATURE_SCREEN;
        drawTemperatureScreen();
    }
    // If the tare button is pressed
    else if (tare_btn.justPressed()) {
        tare_btn.drawButton(true);
        blue_btn.drawButton();
        red_btn.drawButton();
        screenState = MAIN_MENU;
        scale.tare();
        drawMainMenu();
    }
}

// Handle actions on the setpoint screens
void handleNumpadButtons(bool down) {
    back_btn.press(down && back_btn.contains(pixel_x, pixel_y));
    clear_btn.press(down && clear_btn.contains(pixel_x, pixel_y));
    dot_btn.press(down && dot_btn.contains(pixel_x, pixel_y));

    // If the back button is pressed
    if (back_btn.justPressed()) {
        screenState = MAIN_MENU;
        drawMainMenu();
    }
    // If the clear button is pressed
    else if (clear_btn.justPressed()) {
        if (numpadIndex > 0) {
            numpadInput[--numpadIndex] = '\0';
            drawNumpadScreen(screenState);
        }
    }
    // If the decimal-point button is pressed
    else if (dot_btn.justPressed()) {
        if (numpadIndex < 4 && !strchr(numpadInput, '.')) {
            numpadInput[numpadIndex++] = '.';
            numpadInput[numpadIndex] = '\0';
            drawNumpadScreen(screenState);
        }
    }

    // Check whether a numeric button is pressed
    for (int i = 0; i < 11; i++) {
        num_btns[i].press(down && num_btns[i].contains(pixel_x, pixel_y));
        if (num_btns[i].justPressed()) {
            num_btns[i].drawButton(true);
            handleNumpadPress(i);
        }
    }
}

// Manage the file header
bool headersWritten = false;
void SDWriteHeader() {
    if (!headersWritten) {
        myFile = SD.open("DATA.txt", FILE_WRITE);
        if (myFile) {
             myFile.println("Date,Time,Temperature,Humidity,Mass");
            myFile.close();
            headersWritten = true;
        }
    }
}

// Write the date and time to the serial monitor and SD card
void SDTime() {
  DateTime now = clock.now();
  myFile = SD.open("DATA.txt", FILE_WRITE);
  if (myFile) {
    myFile.print(now.year(), DEC);
    myFile.print('/');
    myFile.print(now.month(), DEC);
    myFile.print('/');
    myFile.print(now.day(), DEC);
    myFile.print(',');
    myFile.print(now.hour(), DEC);
    myFile.print(':');
    myFile.print(now.minute(), DEC);
    myFile.print(':');
    myFile.print(now.second(), DEC);
    myFile.print(",");
  }
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.println(now.day(), DEC);
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);
  myFile.close();
  delay(1000); 
    
}

// Read the temperature
float ReadTemperature(){

  float temperature = sht.getTemperature();
  return temperature;

}

void ControlVentilator(float humidity, float temperature) {
  if (humidity < humiditySetPoint || temperature < temperatureSetPoint) {
    digitalWrite(RELAY_PIN_VENTI, HIGH);  // Turn the fan on
    Serial.println("Fan turned on");
  } else {
    digitalWrite(RELAY_PIN_VENTI, LOW);  // Turn the fan off
    Serial.println("Fan turned off");
  }
}


// Display temperature on the screen
void ScreenTemperature(float temperature) {
    tft.fillRect(100, 50, 60, 50, WHITE); // Temperature display area
    // Font settings
    tft.setTextSize(2);
    tft.setTextColor(BLACK); // Text color for updated values
    tft.setCursor(20, 50);
    tft.print("Tempe: ");
    tft.setCursor(100, 50);
    tft.print(temperature);
    tft.setCursor(175, 50);
    tft.println(" C");
    
}


// Write temperature data to the SD card
void SDTemperature(float temperature) { 
  Serial.print("Temperature: "); 
  Serial.print(temperature);
  Serial.println(" *C");
  myFile = SD.open("DATA.txt", FILE_WRITE);
  if (myFile) {
    Serial.println("open with success");
    myFile.print(temperature);
    myFile.print(",");
  }
  myFile.close();
}

// Temperature control function

void ReguTemperature(float temperature, float humidity){
  
  
  if (temperature < temperatureSetPoint) {
    digitalWrite(RELAY_PIN_RESI, HIGH);  // Turn the heating element on
    
    Serial.println("Heating on");
  } else {
    digitalWrite(RELAY_PIN_RESI, LOW);  // Turn the heating element off
    
    Serial.println("Heating off");

    // Check whether the fan should be activated
  ControlVentilator(humidity, temperature);
  }
}

// Read the relative humidity
float ReadHumidity(){

  float humidity = sht.getHumidity();
  return humidity;

}

// Display relative humidity on the screen
void ScreenHumidity(float humidity) {
    tft.fillRect(100, 100, 60, 50, WHITE); // Humidity display area
    // Font settings
    tft.setTextSize(2);
    tft.setTextColor(BLACK); // Text color for updated values
    tft.setCursor(20, 100);
    tft.print("Humid: ");
    tft.setCursor(100, 100);
    tft.print(humidity);
    tft.setCursor(175, 100);
    tft.println(" %");
    
}

// Write humidity data to the SD card
void SDHumidity(float humidity) {
  
  Serial.print("Humidity: "); 
  Serial.print(humidity);
  Serial.println(" % ");
   
  
  myFile = SD.open("DATA.txt", FILE_WRITE);
  if (myFile) {
    Serial.println("open with success");
    myFile.print(humidity);
    myFile.print(",");
  }
  myFile.close();
}

// Humidity control function
void ReguHumidity(float humidity, float temperature){
  
  if (humidity< humiditySetPoint) {
    digitalWrite(HUMIDIFICATEUR_PIN, HIGH);  // Turn the humidifier on
    
    
      
    Serial.println("Humidifier on");
  } else {
    digitalWrite(HUMIDIFICATEUR_PIN, LOW);  // Turn the humidifier off
    


    Serial.println("Humidifer off");

    // Check whether the fan should be activated
  ControlVentilator(humidity, temperature);
  }
}
// Control the fan

// Read the mass
float ReadMasse(){
  float masse = scale.get_units(10);
  return masse;
}

// Display the mass on the screen
void ScreenMasse(float masse) {
    tft.fillRect(100, 150, 85, 50, WHITE); // Mass display area
    // Font settings
    tft.setTextSize(2);
    tft.setTextColor(BLACK); // Text color for updated values
    tft.setCursor(20, 150);
    tft.print("Mass: ");
    tft.setCursor(100, 150);
    tft.print(masse, 1);
    tft.setCursor(175, 150);
    tft.println(" g");
    Serial.print(masse, 1);
    
}

// Write mass data to the SD card
void SDMasse(float masse) {
  Serial.print("Mass "); 
  Serial.print(masse);
  Serial.println(" g ");
   
  
  myFile = SD.open("DATA.txt", FILE_WRITE);
  if (myFile) {
    Serial.println("open with success");
    myFile.print(masse);
    myFile.println(",");
  }
  myFile.close();
}


// Setup
void setup(void)
{
  
// Initialize serial communication
    initSerial();

    // Initialize the I2C bus
    initI2C();

    // Initialize the components
    bool success = false;
    while (!success) {
        success = initializeComponents(); // Initialize the components
        if (!success) {
            displayError("Initialization error!");
        }
    }

    // Configure relay and other output pins
    RelaisPins();

    // Display the welcome message
    WelcomeMessage();

    // Draw the main menu at startup
    drawMainMenu();

}

void loop() {
    // Check whether the touchscreen is pressed
    bool down = Touch_getXY();

    // Handle actions according to the current screen state
    if (screenState == MAIN_MENU) {
        handleMainMenuButtons(down);
    } else if (screenState == HUMIDITY_SCREEN || screenState == TEMPERATURE_SCREEN) {
        handleNumpadButtons(down);
    }

    unsigned long currentTime = millis();
    if (currentTime - lastUpdateTime >= updateInterval)
    {
        // Update sensor values only when the sampling interval has elapsed
        lastUpdateTime = currentTime;

        // Read temperature and humidity from the SHT sensor
        sht.read();
        float temperature = sht.getTemperature();
        float humidity = sht.getHumidity();
        
        // Read the load cell value
        float masse = scale.get_units(10);

        // Update the displayed values
        // Clear only the previous values by drawing background-colored rectangles
        if (screenState == MAIN_MENU)
        {
            tft.fillRect(100, 50, 60, 50, WHITE); // Temperature display area
            tft.fillRect(100, 100, 60, 50, WHITE); // Humidity display area
            tft.fillRect(100, 150, 85, 50 , WHITE); // Mass display area

            // Display the updated values
            tft.setTextSize(2);
            tft.setTextColor(BLACK); // Text color for updated values
        
            // Display temperature on the screen
            tft.setCursor(20, 50);
            tft.print("Tempe: ");
            tft.setCursor(100,50);
            tft.print(temperature);
            tft.setCursor(175,50);
            tft.println(" C");
            Serial.print(temperature);

            // Display humidity on the screen
            tft.setCursor(20, 100);
            tft.print("Humid: ");
            tft.setCursor(100,100);
            tft.print(humidity);
            tft.setCursor(175,100);
            tft.println(" %");
            Serial.print(humidity);

            // Display mass on the screen
            tft.setCursor(20, 150);
            tft.print("Mass: ");
            tft.setCursor(100,150);
            tft.print(masse, 1);
            tft.setCursor(175,150);
            tft.println(" g");
            Serial.print(masse,1);
            // Control the outputs as required
            ReguTemperature(temperature,humidity);
            ReguHumidity(humidity,temperature);
            // Save the measurements to the SD card
            SDTime();
            SDTemperature(temperature);
            SDHumidity(humidity);
            SDMasse(masse);
        }
    }
}
