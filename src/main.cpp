/* Diatonic accordion midi project */
//TODO : change key->midi handling (if a key should be played, add 1 to it's value and play it if the value is >0)

#include "Arduino.h"

//Ugly hack to solve issue with functions defined twice
#undef max
#undef min



//-----------------------------------
//Libs & header import
//-----------------------------------
#include <MIDI.h>
#include <mt32.h>
#include "midi_helper.h"
#include <SFE_BMP180.h>
#include <Wire.h>
#include <list>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_BMP085.h>
//OLED lib
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

//Menu lib
#include <menu.h>
#include <menuIO/SSD1306AsciiOut.h>
#include <menuIO/stringIn.h>

//-----------------------------------
// Menu/OLED config
//-----------------------------------
#define OLED_I2C_ADDRESS 0x3C
using namespace Menu;

//Uncomment this to use large font
// #define LARGE_FONT Verdana12

//Max menu depth
#define MAX_DEPTH 2

 /*Do not change the values(recomended)*/
#ifdef LARGE_FONT
    #define menuFont LARGE_FONT
    #define fontW 8
    #define fontH 16
#else
    // #define menuFont System5x7
    #define menuFont lcd5x7
    #define fontW 5
    #define fontH 8
#endif


//-----------------------------------
//Program config
//-----------------------------------
#define ROW_NUMBER_R 11
#define COLUMN_NUMBER_R 3

//Other keys
#define KEY_MENU 9
#define KEY_MISC 10

//Useful define
#define PUSH 1
#define NOPUSH 2
#define PULL 0

//Useful flags
// #define DEBUG
#define OCTAVED 1

//-----------------------------------
//Global variables
//-----------------------------------
//Contains pressed keys
uint32_t keys_md;
uint16_t keys_md_row[3];


//Contain the information 'is button pressed ?' for whole matrix
bool R_press[COLUMN_NUMBER_R][ROW_NUMBER_R];
//Contain the information 'was button pressed last iteration?' for whole matrix
bool R_prev_press[COLUMN_NUMBER_R][ROW_NUMBER_R];

//Notes definitions for pull
uint8_t R_notesT[COLUMN_NUMBER_R][ROW_NUMBER_R] = {
        {mid_F3+1, mid_A3  , mid_C4, mid_E4  , mid_F4+1, mid_A4  , mid_C5  , mid_E5  , mid_F5+1, mid_G5  , mid_C6  }, //1rst row
        {mid_G3  , mid_B3  , mid_D4, mid_F4  , mid_A4  , mid_B4  , mid_D5  , mid_F5  , mid_A5  , mid_B5  , mid_D6  }, //2nd row
        {mid_B3-1, mid_C4+1, mid_G4, mid_A4-1, mid_B4-1, mid_C5+1, mid_E5-1, mid_A5-1, mid_B5-1, mid_C6+1, mid_E6-1}};//3rd row
//Notes definitions for push
uint8_t R_notesP[COLUMN_NUMBER_R][ROW_NUMBER_R] = {
        {mid_D3  , mid_G3  , mid_B3  , mid_D4  , mid_G4  , mid_B4  , mid_D5  , mid_G5  , mid_B5  , mid_D6  , mid_G6  }, //1rst row
        {mid_E3  , mid_G3  , mid_C4  , mid_E4  , mid_G4  , mid_C5  , mid_E5  , mid_G5  , mid_C6  , mid_E6  , mid_G6  }, //2nd row
        {mid_A3-1, mid_B3-1, mid_E4-1, mid_A4-1, mid_B4-1, mid_E5-1, mid_A5-1, mid_B5-1, mid_E6-1, mid_A6-1, mid_B6-1}};//3rd row

//Variation for BC, add 4 semitone to 1rst ROW
    // uint8_t R_notesT[COLUMN_NUMBER_R][ROW_NUMBER_R] = {
    // 		{mid_F3+1+4, mid_A3+4, mid_C4+4, mid_E4+4, mid_F4+1+4, mid_A4+4, mid_C5+4, mid_E5+4, mid_F5+1+4, mid_G5+4, mid_C6+4},
    // 	 	{mid_G3, mid_B3, mid_D4, mid_F4, mid_A4, mid_B4, mid_D5, mid_F5, mid_A5, mid_B5, mid_D6},
    // 	 	{mid_B3-1, mid_C4+1, mid_G4, mid_A4-1, mid_B4-1, mid_C5+1, mid_E5-1, mid_A5-1, mid_B5-1, mid_C6+1, mid_E6-1}};
    // uint8_t R_notesP[COLUMN_NUMBER_R][ROW_NUMBER_R] = {
    // 		{mid_D3+4, mid_G3+4, mid_B3+4, mid_D4+4, mid_G4+4, mid_B4+4, mid_D5+4, mid_G5+4, mid_B5+4, mid_D6+4, mid_G6+4},
    // 	 	{mid_E3, mid_G3, mid_C4, mid_E4, mid_G4, mid_C5, mid_E5, mid_G5, mid_C6, mid_E6, mid_G6},
    // 	 	{mid_A3-1, mid_B3-1, mid_E4-1, mid_A4-1, mid_B4-1, mid_E5-1, mid_A5-1, mid_B5-1, mid_E6-1, mid_A6-1, mid_B6-1}};


//Useful lists
std::list<int> notes_to_play;
std::list<int> notes_to_remove;

//MCP23X17
Adafruit_MCP23X17 mcp_md_0;
Adafruit_MCP23X17 mcp_md_1;


//OLED
SSD1306AsciiWire oled;
char str_oled[128/fontW];

//pressure stuff
uint8_t volume, volume_prev;
uint8_t bellow, bellow_not_null, bellow_prev; //Contains info if current bellow direction is push or pull
SFE_BMP180 bmp_in;
double p_tare;
double p_offset;
double T, P;
long t_start;
float min_pressure = 0.2;
float max_pressure = 15;
char bmp_status;
bool waiting_t=0;
bool waiting_p=0;


//Midi creation
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDIUSB);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDIPI);

//-----------------------------------
//Menu definition
//-----------------------------------
uint8_t volume_attenuation=0;
uint8_t mt32_rom_set=0;
uint8_t mt32_soundfont=0;
bool mt32_synth=0;
bool debug_oled=0;
bool dummy=0;

// result menu_adjust_volume() {
//     midi_send_master_volume(master_volume, MIDIPI);
//     midi_send_master_volume(master_volume, MIDIUSB);
//     return proceed;
// }
result menu_mt32_switch_rom_set() {
    mt32_switch_rom_set(mt32_rom_set, MIDIPI);
    return proceed;
}
result menu_mt32_switch_soundfont() {
    mt32_switch_soundfont(mt32_soundfont, MIDIPI);
    return proceed;
}
result menu_mt32_switch_synth() {
    mt32_switch_synth(mt32_synth, MIDIPI);
    return proceed;
}


//MT32 submenu
TOGGLE(mt32_synth, synthctrl, "Synth     : ", doNothing, noEvent, noStyle
       , VALUE("SF", HIGH, menu_mt32_switch_synth, noEvent)
       , VALUE("MT32", LOW, menu_mt32_switch_synth, noEvent)
      );
TOGGLE(mt32_rom_set, romctrl, "ROM       : ", doNothing, noEvent, noStyle
       , VALUE("MT32_OLD", 0x00, menu_mt32_switch_rom_set, noEvent)
       , VALUE("MT32_NEW", 0x01, menu_mt32_switch_rom_set, noEvent)
       , VALUE("CM_32L", 0x02, menu_mt32_switch_rom_set, noEvent)
      );
MENU(mt32_config, "MT32 config", doNothing, noEvent, wrapStyle
     , FIELD(mt32_soundfont, "SoundFont :", "", 0, 10, 1, 1, menu_mt32_switch_soundfont, anyEvent, wrapStyle)
     , SUBMENU(synthctrl)
     , SUBMENU(romctrl)
    );

//Debug submenu
TOGGLE(debug_oled, debugoledctrl, "Debug OLED : ", doNothing, noEvent, noStyle
       , VALUE("ON", HIGH, doNothing, noEvent)
       , VALUE("OFF", LOW, doNothing, noEvent)
      );
TOGGLE(dummy, dummyctrl, "Dummy : ", doNothing, noEvent, noStyle
       , VALUE("ON", HIGH, doNothing, noEvent)
       , VALUE("OFF", LOW, doNothing, noEvent)
      );
MENU(debug_config, "Debug menu", doNothing, noEvent, wrapStyle
     , SUBMENU(debugoledctrl)
     , SUBMENU(dummyctrl) //for some reason, menu must have at least 2 elements
    );
//Keyboard layout submenu TODO

//Main menu
TOGGLE(volume_attenuation, volmumectrl, "Volume att: ", doNothing, noEvent, noStyle
       , VALUE("-0", 0, doNothing, noEvent)
       , VALUE("-16", 16, doNothing, noEvent)
       , VALUE("-32", 32, doNothing, noEvent)
       , VALUE("-48", 48, doNothing, noEvent)
       , VALUE("-64", 64, doNothing, noEvent)
      );
MENU(mainMenu, "Main menu", doNothing, noEvent, wrapStyle
     , SUBMENU(volmumectrl)
     , SUBMENU(mt32_config)
     , SUBMENU(debug_config)
    );



//describing a menu output device without macros
//define at least one panel for menu output
const panel panels[] MEMMODE = {{0, 0, 128 / fontW, 64 / fontH}};
navNode* nodes[sizeof(panels) / sizeof(panel)]; //navNodes to store navigation status
panelsList pList(panels, nodes, 1); //a list of panels and nodes
idx_t tops[MAX_DEPTH] = {0, 0}; //store cursor positions for each level

#ifdef LARGE_FONT
SSD1306AsciiOut outOLED(&oled, tops, pList, 8, 2); //oled output device menu driver
#else
SSD1306AsciiOut outOLED(&oled, tops, pList, 5, 1); //oled output device menu driver
#endif

menuOut* constMEM outputs[]  MEMMODE  = {&outOLED}; //list of output devices
outputsList out(outputs, 1); //outputs list

//Menu input
stringIn<0> strIn;//buffer size: use 0 for a single byte
noInput none;//uses its own API
NAVROOT(nav,mainMenu,MAX_DEPTH,none,out);


//###############################################
//Initial setup
//###############################################
void setup()
{

    //Init both MIDI
    MIDIUSB.begin(1);
    MIDIPI.begin(1);
    Serial.begin(115200);
    Serial1.begin(115200);

    //Set I2C frequ
    Wire.begin();  
    Wire.setClock(400000);

    //Init pressure sensor
    #ifdef DEBUG
        Serial.println("Init BMP180\n");
    #endif
     if (!bmp_in.begin()) {
        Serial.println("BMP180 init fail\n\n");
        while (1); // Pause forever.
    }

    //Init OLED & menu
    oled.begin(&Adafruit128x64, OLED_I2C_ADDRESS);
    oled.setFont(menuFont);
    oled.setCursor(0, 0);
    oled.print("Diato MIDI");
    oled.setCursor(0, 2);
    oled.print("Enjoy !");
    delay(2000);
    oled.clear();

    //Init MCP
    #ifdef DEBUG
        Serial.println("Init mcp_md_0\n");
    #endif
    if (!mcp_md_0.begin_I2C()) {
        Serial.println("Error mcp_md_0.");
        while (1);
    }
    #ifdef DEBUG
        Serial.println("Init mcp_md_1\n");
    #endif
    if (!mcp_md_1.begin_I2C(0x24)) {
        Serial.println("Error mcp_md_1.");
        while (1);
    }

    //Configure MCPs to all input PULLUP
    for(int i=0;i<16;i++){
        mcp_md_0.pinMode(i, INPUT_PULLUP);
        mcp_md_1.pinMode(i, INPUT_PULLUP);
    }

    //Configure GPIO to INPUT
    pinMode(KEY_MENU, INPUT_PULLUP);
    pinMode(KEY_MISC, INPUT_PULLUP);

    //init some variable
    memset(R_prev_press, 0, sizeof(R_prev_press));
    volume_prev=0;
    bellow_prev=NOPUSH;

    //Mesure initial pressure at boot
    p_tare=0;
    bmp_status = bmp_in.startTemperature();
    if (bmp_status == 0) {Serial.println("error retrieving pressure measurement\n");}
    delay(bmp_status);
    bmp_status = bmp_in.getTemperature(T);
    if (bmp_status == 0) {Serial.println("error retrieving pressure measurement\n");}

    // Start a pressure measurement:
    bmp_status = bmp_in.startPressure(3);
    if (bmp_status == 0) {Serial.println("error retrieving pressure measurement\n");}

    t_start = millis();
    while (millis() - t_start < bmp_status);
    bmp_status = bmp_in.getPressure(P, T);
    if (bmp_status == 0) {Serial.println("error retrieving pressure measurement\n");}
    p_tare=P;

    mt32_switch_synth(MT32_SOUNDFONT, MIDIPI);

    #ifdef DEBUG
        Serial.println("INIT OK.");
    #endif
}

//###############################################
//Main loop
//###############################################
void loop()
{

    //-----------------------------------
    // Initialise loop
    //-----------------------------------
    //remember old pressed touch
    memcpy(R_prev_press, R_press, sizeof(R_press));
    //Zero some variables in doubt
    notes_to_play.clear();
    notes_to_remove.clear();
    memset(R_press, 0, sizeof(R_press));

    //-----------------------------------
    // Temp & pressure mesurement
    //-----------------------------------
    //This process is semi asynchronous from the loop thanks to variables waiting_t, waiting_p
    if(!waiting_t && !waiting_p){
        // Start temp mesurement
        bmp_status = bmp_in.startTemperature();
        if (bmp_status == 0) {Serial.println("error retrieving temp measurement\n");}
        t_start=millis();
        waiting_t=true;
    } else if(waiting_t && ((millis()-t_start) > bmp_status) ){
        // Retrieve the completed temperature measurement:
        bmp_status = bmp_in.getTemperature(T);
        if (bmp_status == 0) {Serial.println("error retrieving temp measurement\n");}

        // Start a pressure measurement:
        bmp_status = bmp_in.startPressure(3);
        t_start = millis();
        if (bmp_status == 0) {Serial.println("error retrieving pressure measurement\n");}
        waiting_t=false;
        waiting_p=true;
    } else if (waiting_p && ((millis() - t_start) > bmp_status) ) {
        // Retrieve the completed pressure measurement:
        bmp_status = bmp_in.getPressure(P, T);
        waiting_p=false;
        if (bmp_status == 0) {Serial.println("error retrieving pressure measurement\n");}

        // Update pressure & volume variables
        p_offset = P - p_tare;
        //Trunk the pressure if too big/low
        if (p_offset > max_pressure) {p_offset = max_pressure;}
        if (p_offset < -max_pressure) {p_offset = -max_pressure;}
        //If no pressure, no volume
        if (abs(p_offset) < min_pressure) {
            bellow = NOPUSH;
            volume = 0;
        } else {
            if (p_offset < 0) { //PULL
                bellow          = PULL;
            } else {            //PUSH
                bellow          = PUSH;
            }
            bellow_not_null = bellow;
            //volume calculation :
            p_offset = abs(p_offset);
            volume = uint8_t((log(float(p_offset) / 55) + 6) * 27) ;
            if (volume_attenuation>=volume) {
                volume=0;
            } else {
                volume-=volume_attenuation;
            }
        }
    }
    
    

    // bellow          = PUSH;
    // bellow_not_null = bellow;
    // volume = 100 ;

    //-----------------------------------
    // Key acquisition from MCP
    //-----------------------------------
    keys_md =  (uint32_t)  mcp_md_0.readGPIO(1) & 0xff;
    keys_md |= ((uint32_t) mcp_md_0.readGPIO(0) & 0xff)<<8;
    keys_md |= ((uint32_t) mcp_md_1.readGPIO(1) & 0xff)<<16;
    keys_md |= ((uint32_t) mcp_md_1.readGPIO(0) & 0xff)<<24;

    //Conversion to R_press
    keys_md_row[0] =  keys_md >> (10+ROW_NUMBER_R);
    keys_md_row[1] = (keys_md >> 10) & 0x7FF;
    keys_md_row[2] =  keys_md & 0x3FF;
    for(uint8_t i=0;i<COLUMN_NUMBER_R;i++){
        for(uint8_t j=0;j<ROW_NUMBER_R;j++){
            R_press[i][j]=(keys_md_row[i] & (1<<(j)))>>(j);
        }
    }

    //-----------------------------------
    //Menu navigation
    //-----------------------------------
    //For some reason, going low on a FIELD crash the menu, try to avoid it
    if(digitalRead(KEY_MENU)) {
        if (R_press[1][9] && ! R_prev_press[1][9]) {
            strIn.write('-');
            nav.doInput(strIn);
        }
        if (R_press[1][10] && ! R_prev_press[1][10]) {
            strIn.write('+');
            nav.doInput(strIn);
        } 
        if (R_press[2][9] && ! R_prev_press[2][9]) {
            strIn.write('/');
            nav.doInput(strIn);
        } 
        if (R_press[0][9] && ! R_prev_press[0][9]) {
            strIn.write('*');
            nav.doInput(strIn);
        }
        nav.poll();
    }


    //-----------------------------------
    // Prepare MIDI message
    //-----------------------------------
   for (size_t i = 0; i < COLUMN_NUMBER_R; i++) {
        for (size_t j = 0; j < ROW_NUMBER_R; j++) {
            //Add and remove note depending on bellow direction and previous direction
            if (bellow_prev == PUSH && bellow == PULL) {
                if (R_press[i][j]) {
                    notes_to_remove.push_back(R_notesP[i][j]);
                    notes_to_play.push_back(R_notesT[i][j]);
                }
            } else if (bellow_prev == PULL && bellow == PUSH) {
                if (R_press[i][j]) {
                    notes_to_remove.push_back(R_notesT[i][j]);
                    notes_to_play.push_back(R_notesP[i][j]);
                }
            }
            if (R_prev_press[i][j]) {
                if (!R_press[i][j]) { //remove note if touch isn't pressed anymore
                    if (bellow_not_null == PUSH || bellow_prev == PUSH)
                    {
                        notes_to_remove.push_back(R_notesP[i][j]);
                    }
                    if (bellow_not_null == PULL || bellow_prev == PULL)
                    {
                        notes_to_remove.push_back(R_notesT[i][j]);
                    }
                }
            } else if (!R_prev_press[i][j]) { //Add notes
                if (R_press[i][j]) {
                    if (bellow_not_null == PUSH) {
                        notes_to_play.push_back(R_notesP[i][j]);
                    }
                    else {
                        notes_to_play.push_back(R_notesT[i][j]);
                    }
                }
            }
        }
    }

    //Uniquify the list to minimize number of message
    notes_to_play.sort();
    notes_to_play.unique();
    notes_to_remove.sort();
    notes_to_remove.unique();
    //-----------------------------------
    // Send MIDI message
    //-----------------------------------
    while (!notes_to_remove.empty()) {
        #ifndef DEBUG
            MIDIUSB.sendNoteOff(notes_to_remove.back()+12*OCTAVED, 0, 1);
        #endif
        MIDIPI.sendNoteOff(notes_to_remove.back()+12*OCTAVED, 0, 1);

        notes_to_remove.pop_back();
    }
    while (!notes_to_play.empty()) {
        #ifndef DEBUG
            MIDIUSB.sendNoteOn(notes_to_play.back()+12*OCTAVED, 100, 1);
        #endif
        MIDIPI.sendNoteOn(notes_to_play.back()+12*OCTAVED, 100, 1);
        notes_to_play.pop_back();
    }
    if (bellow != NOPUSH) {bellow_prev = bellow;}

  
    if(volume_prev!=volume){
        #ifndef DEBUG
            MIDIUSB.sendControlChange(7,volume, 1);
        #endif
        MIDIPI.sendControlChange(7,volume, 1);
    }
    volume_prev=volume;


    if(debug_oled){
        //Fancy displays (take a while and eat screen space so we limit it to debug)
        //Volume
        oled.setCursor(0, 64/fontH-1);
        for (uint8_t i = 0; i < 128/fontW ; i++) {
            if (volume/(fontW+1) > i) {
                if(bellow==PUSH) {
                    str_oled[i] = '+';
                } else {
                    str_oled[i] = '-';
                }
            } else {
                str_oled[i] = ' ';
            }
        }
        oled.print(str_oled);
        for (uint8_t i = 0; i < 128/fontW ; i++) {
            str_oled[i] = ' ';
        }
        //Keyboard right hand
        for (uint8_t i = 0; i < COLUMN_NUMBER_R; i++) {
            oled.setCursor(0, 64/fontH-4+i);
            for (uint8_t j = 0;  j < ROW_NUMBER_R; j++) {
                if(R_press[i][j]){
                    str_oled[j] = '+';
                } else {
                    str_oled[j] = '-';
                }
            }
            oled.print(str_oled);
        }
    }


    #ifdef DEBUG
        Serial.println("DEBUG DURATION");

        Serial.println("init");
        Serial.println(t__dbg_init     - t__dbg_loop    , DEC);
        Serial.println("press1");
        Serial.println(t__dbg_press   - t__dbg_init      , DEC);
        Serial.println("key");
        Serial.println(t__dbg_key      - t__dbg_press    , DEC);
        Serial.println("menu");
        Serial.println(t__dbg_menu     - t__dbg_key       , DEC);
        Serial.println("press wait");
        Serial.println(t__dbg_prep_midi - t__dbg_menu      , DEC);
        Serial.println("send midi");
        Serial.println(t__dbg_send_midi- t__dbg_prep_midi , DEC);
        Serial.println("total");
        Serial.println(t__dbg_send_midi- t__dbg_loop      , DEC);

        Serial.println("END\n\r");
        // Serial.write(27);       // ESC command
        // Serial.print("[2J");    // clear screen command
        // Serial.write(27);
        // Serial.print("[H");     // cursor to home command
        // for (size_t j = 0; j < COLUMN_NUMBER_R; j++) {
        //     for (size_t i = 0; i < ROW_NUMBER_R; i++) {
        //         if(R_press[j][i]) {
        //             Serial.print("X");
        //         } else {
        //             Serial.print("-");
        //         }
        //     }
        //     Serial.print("   ");
        // }
        // Serial.print("\n\r");
        delay(1000);
    #endif
}
