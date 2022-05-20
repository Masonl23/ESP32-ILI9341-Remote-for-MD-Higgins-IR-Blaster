//The file used for creating keypad buttons

//height and width of the screen
#define SCREEN_WIDTH 239
#define SCREEN_HEIGHT 319

//Custom color created of green
#define MY_GREEN 0x9fa8
/**
 * @brief Names for the buttons on the LCD screen
 * 
 */
enum BUTTON_NAMES{
    TV_POWER, POWER, AMP_POWER,
    MUSIC, UP_VOL,
    MUTE,
    TV, DOWN_VOL,
    NUM_BUTTONS
    
};

/**
 * @brief Names for the state of the screen whether its on or off
 * 
 */
enum DISPLAY_STATE{
    DISPLAY_ON, DISPLAY_OFF, NUM_DISPLAY_STATES
};

//Starts the WiFi modules and connects to the network
void initWiFi();

//Creates the button sections on the screen
void initButtons();

//Calibrates the touchscreen
void touch_calibrate();

//Sends URL request to the IR blaster
void urlRequest(String urlString);

//Loop pinned to core 1 that handles GUI
void GUIloop(void *parameter);

//Loop pinned to core 0 that handles WiFi and URL commands
void Wifiloop(void *parameter);

