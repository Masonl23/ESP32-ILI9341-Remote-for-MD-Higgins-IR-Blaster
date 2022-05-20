
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>
#include "FS.h"
#include <Arduino.h>
#include <ESP_WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Custom files included
#include "GUIfile.h"
#include "images.h"

// Calibration for the touchscreen
#define CALIBRATION_FILE "/TouchCalData12"
#define REPEAT_CAL false // set to true if you want to calibrate each power on

// Controls the led pin, which turns the LCD LED on and off
#define LCD_POWER_PIN 17

#define ONE_SECOND 1000

// State of the LCD
DISPLAY_STATE state;

TFT_eSPI tft = TFT_eSPI();            // Invoke custom library
TFT_eSPI_Button buttons[NUM_BUTTONS]; // Instantiate the buttons

// Add wifi parameters... i know i should use wifi manager but gave me issues on platform io and got lazy
const char *ssid = "SSID";
const char *password = "PASSWORD!";

// URLS for the ir commands to be sent to the IR blaster hub
const char *volUpURL = "";
const char *volDownURL = "";
const char *muteURL = "";
const char *TVPowerURL = "";
const char *AMPPowerURL ="";
const char *musicSelectURL = "";
const char *TVSelectURL = "";

// For using both cores of the esp32, this is mitigate lag in the USER interface
TaskHandle_t TFThelper;  // TFT helper assigns one core to only deal with graphics
TaskHandle_t WifiHelper; // WiFiHelper assigns another core to only deal with WiFi commands
QueueHandle_t queue;     // Queue used between the cores, when a button is pressed send the notification to the queue so the other core can handle

void setup(void)
{
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_WHITE);

  Serial.begin(9600); // For debug

  // Define pin, set state, and turn the LCD on
  pinMode(LCD_POWER_PIN, OUTPUT);
  digitalWrite(LCD_POWER_PIN, HIGH);
  state = DISPLAY_ON;

  initWiFi();

  // Calibrate the screen if bool is true or there are no files
  touch_calibrate();
  // Push the GUI image onto the TFT display
  tft.setSwapBytes(true);
  // display the background image onto the screen
  tft.pushImage(0, 0, guiWidth, guiHeight, ESP32_Remote_gray);

  // init the buttons onto the screen
  initButtons();

  queue = xQueueCreate(8, sizeof(int)); // create queue for commands to be filled to
  xTaskCreatePinnedToCore(GUIloop, "GUIHelper", 4096, NULL, 0, &TFThelper, 1);
  delay(500);
  xTaskCreatePinnedToCore(Wifiloop, "WiFiHelper", 8192, NULL, 1, &WifiHelper, 0);
}

void loop()
{
  delay(1); // Put something in the loop, nothing is run here
}

/**
 * @brief Starts the connection of the WiFi module.
 * 
 */
void initWiFi()
{
  // begin wifi and print the results
  WiFi.begin(ssid, password);
  tft.println("Connecting...");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    tft.print(".");
  }
  tft.println("");
  tft.print("Connected to WiFi network with IP Address: ");
  tft.println(WiFi.localIP());

  // let the ip display for a second before wiping the screen
  delay(ONE_SECOND);
}

/**
 * @brief Creates the button regions, but does not draw them to the screen. This
 * is because we are using a graphic as the background, we just want the properties
 * of the button to make life easier, such as if a touch was in the button.
 *
 */
void initButtons()
{
  buttons[TV_POWER].initButtonUL(&tft, 0, 0, 80,
                                 86, TFT_WHITE, MY_GREEN, TFT_BLACK, "TV POWER", 1);

  buttons[POWER].initButtonUL(&tft, 80, 0, 80,
                              86, TFT_WHITE, MY_GREEN, TFT_BLACK, "POWER", 1);

  buttons[AMP_POWER].initButtonUL(&tft, 160, 0, 80,
                                  86, TFT_WHITE, MY_GREEN, TFT_BLACK, "AMP POWER", 1);

  buttons[MUSIC].initButtonUL(&tft, 0, 86, 115,
                              100, TFT_WHITE, TFT_WHITE, TFT_BLACK, "MUSIC", 1);

  buttons[TV].initButtonUL(&tft, 0, 186, 115,
                           100, TFT_WHITE, TFT_WHITE, TFT_BLACK, "TV", 1);

  buttons[UP_VOL].initButtonUL(&tft, 115, 87, 125,
                               73, TFT_WHITE, TFT_WHITE, TFT_BLACK, "+", 2);

  buttons[MUTE].initButtonUL(&tft, 115, 160, 125,
                             66, TFT_WHITE, TFT_WHITE, TFT_BLACK, "MUTE", 1);

  buttons[DOWN_VOL].initButtonUL(&tft, 115, 226, 125,
                                 66, TFT_WHITE, TFT_WHITE, TFT_BLACK, "-", 2);
}

/**
 * @brief Calibrates the touchscreen sensor of the TFT display.
 * Taken from the example sketch of tft_espi. Runs if there is not a saved
 * file already on the device
 *
 */
void touch_calibrate()
{
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!SPIFFS.begin())
  {
    Serial.println("Formating file system");
    SPIFFS.format();
    SPIFFS.begin();
  }

  // check if calibration file exists and size is correct
  if (SPIFFS.exists(CALIBRATION_FILE))
  {
    if (REPEAT_CAL)
    {
      // Delete if we want to re-calibrate
      SPIFFS.remove(CALIBRATION_FILE);
    }
    else
    {
      fs::File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f)
      {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL)
  {
    // calibration data valid
    tft.setTouch(calData);
  }
  else
  {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL)
    {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    fs::File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f)
    {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}

/**
 * @brief ESP goes to the URL that is passed into its parameter.
 *
 * @param urlString: The URL the ESP goes to, usually a link for alexa commands
 */
void urlRequest(String urlString)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;

    // Your Domain name with URL path or IP address with path
    http.begin(urlString);
    http.GET();
    // Free resources
    http.end();
  }
  else
  {
    Serial.println("WiFi Disconnected");
  }
}

/**
 * @brief Loop pinned to a core 1 that handles the GUI, handles only graphics,
 * when there is a touch on the screen, this core decides what button was pressed
 * and passes the button to the queue where core 0 handles the URL requests. This is
 * done to reduce the amount of lag on the screen and the user.
 *
 * @param parameter: There is no parameter passed
 */
void GUIloop(void *parameter)
{
  static unsigned long lastInteraction = 0;
  // loop forever
  for (;;)
  {
    // touch coordinates of the screen
    uint16_t touchX = 0, touchY = 0;

    // if theres a valid touch on the screen set true and set the touch coordinates
    bool pressed = tft.getTouch(&touchX, &touchY);

    // If the display i
    if (pressed && state == DISPLAY_OFF)
    {
      digitalWrite(LCD_POWER_PIN, HIGH);
      state = DISPLAY_ON;
      lastInteraction = millis();
      delay(ONE_SECOND);
    }
    else
    {

      // check if any buttons were touched
      for (uint8_t bttn = 0; bttn < NUM_BUTTONS; bttn++)
      {
        if (pressed && buttons[bttn].contains(touchX, touchY))
        {
          buttons[bttn].press(true);
          lastInteraction = millis();
        }
        else
        {
          buttons[bttn].press(false);
        }
      }

      for (uint8_t bttn_index = 0; bttn_index < NUM_BUTTONS; bttn_index++)
      {
        if (buttons[bttn_index].justReleased())
        {
          // cover the screen again with fresh image, covering the user press
          tft.pushImage(0, 0, guiWidth, guiHeight, ESP32_Remote_gray);
        }
        if (buttons[bttn_index].justPressed())
        {
          int button_index = bttn_index;
          tft.fillSmoothCircle(touchX, touchY, 30, TFT_WHITE);
          xQueueSend(queue, &button_index, portMAX_DELAY); // send the command to the queue for other core
        }
      }
    }

    // if theres no input for 5 seconds turn the screen off
    if ((millis() - lastInteraction > ONE_SECOND*10) && state == DISPLAY_ON)
    {
      digitalWrite(LCD_POWER_PIN, LOW);
      state = DISPLAY_OFF;
    }
  }
}

/**
 * @brief Loop pinned to a core 0 that handles the WiFi commands. This core when there is a
 * command sent to the queue this core sends the correct URL to the IR blaster.
 *
 * @param parameter: There is no parameter passed
 */
void Wifiloop(void *parameter)
{
  for (;;)
  {
    int button_index;
    xQueueReceive(queue, &button_index, portMAX_DELAY);
    switch (button_index)
    {
    case TV_POWER:
      // Serial.println("TV POWER PRESSED");
      urlRequest(TVPowerURL);
      break;
    case POWER:
      // Serial.println("POWER PRESSED");
      urlRequest(TVPowerURL);
      urlRequest(AMPPowerURL);
      break;
    case AMP_POWER:
      // Serial.println("AMP POWER PRESSED");
      urlRequest(AMPPowerURL);
      break;
    case MUSIC:
      // Serial.println("MUSIC PRESSED");
      urlRequest(musicSelectURL);
      break;
    case UP_VOL:
      // Serial.println("UP VOL PRESSED");
      urlRequest(volUpURL);
      break;
    case MUTE:
      // Serial.println("MUTE PRESSED");
      urlRequest(muteURL);
      break;
    case TV:
      // Serial.println("TV PRESSED");
      urlRequest(TVSelectURL);
      break;
    case DOWN_VOL:
      // Serial.println("DOWN VOL PRESSED");
      urlRequest(volDownURL);
      break;
    case NUM_BUTTONS:
      break;
    default:
      break;
    }
  }
}