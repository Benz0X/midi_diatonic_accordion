/* Diatonic accordion midi project */
//TODO : change key->midi handling (if a key should be played, add 1 to it's value and play it if the value is >0)
//TODO : refactor code to use multiple files
//TODO : refactor code to enable/disable functionnalities
// Use midinote struct & fonctions ?

#include "Arduino.h"

//Ugly hack to solve issue with functions defined twice
#undef max
#undef min



//-----------------------------------
//Libs & header import
//-----------------------------------
#include <SPI.h> //although SPI is not needed, some dependencies in there needs it
#include <MIDI.h>
#include <SFE_BMP180.h>
#include <Wire.h>
#include <list>
#include <MCP23017.h>
#include "mt32.h"
#include "midi_helper.h"
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

#define ROW_NUMBER_L 6
#define COLUMN_NUMBER_L 4

//Other keys
#define KEY_MENU 9
#define KEY_MISC 10

//Useful define
#define PUSH 1
#define NOPUSH 2
#define PULL 0

//-----------------------------------
//Global variables
//-----------------------------------
//Contains pressed keys
uint32_t keys_rh;
uint32_t keys_lh;
uint16_t keys_rh_row[COLUMN_NUMBER_R];
uint8_t  keys_lh_row[COLUMN_NUMBER_L];


//Contain the information 'is button pressed ?' for right keyboard
bool R_press[COLUMN_NUMBER_R][ROW_NUMBER_R];
bool L_press[COLUMN_NUMBER_L][ROW_NUMBER_L];
//Contain the information 'was button pressed last iteration?' for left keyboard
bool R_prev_press[COLUMN_NUMBER_R][ROW_NUMBER_R];
bool L_prev_press[COLUMN_NUMBER_L][ROW_NUMBER_L];

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

//Notes definitions for left hand (no push/pull distinction as the keyboard layout is a Serafini Darwin whith same sound in push and pull)
uint8_t L_notes[COLUMN_NUMBER_L][ROW_NUMBER_L] = {
        {mid_D3+1, mid_F3  , mid_G3  , mid_A3  , mid_B3  , mid_C3+1 }, //1rst row
        {mid_A3+1, mid_C3  , mid_D3  , mid_E3  , mid_F3+1, mid_G3+1 }, //2nd row
        {mid_D4+1, mid_F4  , mid_G4  , mid_A4  , mid_B4  , mid_C4+1 }, //3nd row
        {mid_A4+1, mid_C4  , mid_D4  , mid_E4  , mid_F4+1, mid_G4+1 }};//4rd row
//Fifth definitions for left hand (so we can build our own fifth chords)
uint8_t L_notes_fifth[COLUMN_NUMBER_L][ROW_NUMBER_L] = {
        {0       , 0       , 0       , 0       , 0       , 0        }, //1rst row
        {0       , 0       , 0       , 0       , 0       , 0        }, //2nd row
        {mid_A4+1, mid_C4  , mid_D4  , mid_E4  , mid_F4+1, mid_G4+1 }, //3nd row
        {mid_F4  , mid_G4  , mid_A4  , mid_B4  , mid_C4+1, mid_D4+1 }};//4rd row

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
std::list<int> notes_to_play_r  ;
std::list<int> notes_to_play_l  ;
std::list<int> notes_to_remove_r;
std::list<int> notes_to_remove_l;

//MCP23X17
#define MCP23017_ADDRESS 0x20
#define MCP23017_RH_0_SUB_ADDRESS 0x0
#define MCP23017_RH_1_SUB_ADDRESS 0x4
#define MCP23017_LH_0_SUB_ADDRESS 0x7
#define MCP23017_LH_1_SUB_ADDRESS 0x6

MCP23017  mcp_rh_0 = MCP23017(MCP23017_ADDRESS | MCP23017_RH_0_SUB_ADDRESS);
MCP23017  mcp_rh_1 = MCP23017(MCP23017_ADDRESS | MCP23017_RH_1_SUB_ADDRESS);
MCP23017  mcp_lh_0 = MCP23017(MCP23017_ADDRESS | MCP23017_LH_0_SUB_ADDRESS);
MCP23017  mcp_lh_1 = MCP23017(MCP23017_ADDRESS | MCP23017_LH_1_SUB_ADDRESS);



//OLED
SSD1306AsciiWire oled;
char str_oled[128/fontW];

//pressure stuff
uint8_t volume, volume_resolved, volume_prev;
uint8_t expression_resolved=127;

//Contains info if current bellow direction is push or pull
uint8_t bellow, bellow_not_null, bellow_prev;
SFE_BMP180 bmp_in;
double p_tare;
double p_offset;
double T, P;
long t_start;
float min_pressure = 0.2;
float max_pressure = 13;
char bmp_status;
bool waiting_t=0;
bool waiting_p=0;


//Midi creation
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDIUSB);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDIPI);

//-----------------------------------
//Menu definition
//-----------------------------------
uint8_t volume_attenuation  = 0;
uint8_t expression          = 127;
uint8_t mt32_rom_set        = 0;
uint8_t mt32_soundfont      = 0;
int8_t  octave              = 1;
uint8_t pressuremode        = 2;
bool    mt32_synth          = 0;
bool    debug_oled          = 0;
bool    dummy               = 0;
bool    reverse_expr_volume = 0;
bool    fifth_enable        = 1;

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
TOGGLE(reverse_expr_volume, reversectrl, "Velo/expr     : ", doNothing, noEvent, noStyle
       , VALUE("NORMAL", LOW, menu_mt32_switch_synth, noEvent)
       , VALUE("INVERTED", HIGH, menu_mt32_switch_synth, noEvent)
      );
MENU(mt32_config, "MT32 config", doNothing, noEvent, wrapStyle
     , FIELD(mt32_soundfont, "SoundFont :", "", 0, 10, 1, 1, menu_mt32_switch_soundfont, anyEvent, wrapStyle)
     , SUBMENU(synthctrl)
     , SUBMENU(romctrl)
     , SUBMENU(reversectrl)
    );

//Debug submenu
TOGGLE(debug_oled, debugoledctrl, "Debug OLED : ", doNothing, noEvent, noStyle
       , VALUE("ON", HIGH, doNothing, noEvent)
       , VALUE("OFF", LOW, doNothing, noEvent)
      );
TOGGLE(pressuremode, pressurectrl, "Pressuremode : ", doNothing, noEvent, noStyle
       , VALUE("LOGNILS", 0, doNothing, noEvent)
       , VALUE("EXPJASON", 1, doNothing, noEvent)
       , VALUE("CUBICVAVRA", 2, doNothing, noEvent)
       , VALUE("CUBICNILS", 3, doNothing, noEvent)
      );

MENU(debug_config, "Debug menu", doNothing, noEvent, wrapStyle
     , SUBMENU(debugoledctrl)
     , SUBMENU(pressurectrl) //for some reason, menu must have at least 2 elements
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
TOGGLE(expression, expressionctrl, "Expression: ", doNothing, noEvent, noStyle
       , VALUE("127", 127, doNothing, noEvent)
       , VALUE("100", 100, doNothing, noEvent)
       , VALUE("72", 72, doNothing, noEvent)
       , VALUE("48", 48, doNothing, noEvent)
       , VALUE("20", 20, doNothing, noEvent)
      );
TOGGLE(octave, octavectrl, "Octaved : ", doNothing, noEvent, noStyle
       , VALUE("-2", -2, doNothing, noEvent)
       , VALUE("-1", -1, doNothing, noEvent)
       , VALUE("2", 2, doNothing, noEvent)
       , VALUE("1", 1, doNothing, noEvent)
       , VALUE("0", 0, doNothing, noEvent)
      );
TOGGLE(fifth_enable, fifthctrl, "Fifth : ", doNothing, noEvent, noStyle
       , VALUE("ON", HIGH, doNothing, noEvent)
       , VALUE("OFF", LOW, doNothing, noEvent)
      );
MENU(mainMenu, "Main menu", doNothing, noEvent, wrapStyle
     , SUBMENU(volmumectrl)
     , SUBMENU(expressionctrl)
     , SUBMENU(mt32_config)
     , SUBMENU(octavectrl)
     , SUBMENU(fifthctrl)
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
//Helper functions
//###############################################
//Function to read the MCP register in burst. If correctly configured (reset value), the MCP will send
// a pair of register in loop, gaining a lot of time
uint16_t read_burst16_mcp(uint8_t addr) {
    uint16_t buff;
    Wire.requestFrom(MCP23017_ADDRESS | addr, 2);

    buff = (Wire.read())<<8;
    buff |= Wire.read();
    return buff;
}

//Functions to determine volume from pressure, several pressure mode are tested here
uint8_t compute_volume(float x){
    if(pressuremode==0) {
        return uint8_t((log(float(x) / 40) + 6) * 25) ;
    } else if (pressuremode==1) {
        return uint8_t((pow(x,2.1)+245)/5+2*x) ;
    } else if (pressuremode==2) {
        return uint8_t(0.1*pow(x-6,3)+x+79) ;
    } else { // if (pressuremode==3) {
        //0.07579105*x*x*x-2.00077888*x*x+22.258*x+4.993545
        return uint8_t(0.07579105*x*x*x-2.00077888*x*x+22.258*x+4.993545) ;
    }
}

//This will remap the vector from the key acquisition to the physical buttons as it is easier to handle
void remap_left_keys(uint32_t key_in, uint8_t* out_array){
    for (uint8_t i = 0; i < COLUMN_NUMBER_L; i++) {
        out_array[i]=0;
    }
    out_array[0] |= ((key_in&(0x1 << 12)) ?  0x1<<0 : 0 );
    out_array[0] |= ((key_in&(0x1 << 17)) ?  0x1<<1 : 0 );
    out_array[0] |= ((key_in&(0x1 << 16)) ?  0x1<<2 : 0 );
    out_array[0] |= ((key_in&(0x1 << 21)) ?  0x1<<3 : 0 );
    out_array[0] |= ((key_in&(0x1 << 14)) ?  0x1<<4 : 0 );
    out_array[0] |= ((key_in&(0x1 << 19)) ?  0x1<<5 : 0 );
    out_array[1] |= ((key_in&(0x1 << 22)) ?  0x1<<0 : 0 );
    out_array[1] |= ((key_in&(0x1 << 15)) ?  0x1<<1 : 0 );
    out_array[1] |= ((key_in&(0x1 << 20)) ?  0x1<<2 : 0 );
    out_array[1] |= ((key_in&(0x1 << 13)) ?  0x1<<3 : 0 );
    out_array[1] |= ((key_in&(0x1 << 18)) ?  0x1<<4 : 0 );
    out_array[1] |= ((key_in&(0x1 << 23)) ?  0x1<<5 : 0 );
    out_array[2] |= ((key_in&(0x1 <<  6)) ?  0x1<<0 : 0 );
    out_array[2] |= ((key_in&(0x1 << 11)) ?  0x1<<1 : 0 );
    out_array[2] |= ((key_in&(0x1 <<  4)) ?  0x1<<2 : 0 );
    out_array[2] |= ((key_in&(0x1 <<  9)) ?  0x1<<3 : 0 );
    out_array[2] |= ((key_in&(0x1 <<  2)) ?  0x1<<4 : 0 );
    out_array[2] |= ((key_in&(0x1 <<  7)) ?  0x1<<5 : 0 );
    out_array[3] |= ((key_in&(0x1 << 10)) ?  0x1<<0 : 0 );
    out_array[3] |= ((key_in&(0x1 <<  0)) ?  0x1<<1 : 0 );
    out_array[3] |= ((key_in&(0x1 <<  5)) ?  0x1<<2 : 0 );
    out_array[3] |= ((key_in&(0x1 <<  1)) ?  0x1<<3 : 0 );
    out_array[3] |= ((key_in&(0x1 <<  3)) ?  0x1<<4 : 0 );
    out_array[3] |= ((key_in&(0x1 <<  8)) ?  0x1<<5 : 0 );
}

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

    //Init OLED & menu
    oled.begin(&Adafruit128x64, OLED_I2C_ADDRESS);
    oled.setFont(menuFont);
    oled.setCursor(0, 0);
    oled.print("Diato MIDI");
    oled.setCursor(0, 2);
    oled.print("Enjoy !");
    oled.setCursor(0, 64/fontH-1);

    //Init pressure sensor
    #ifdef DEBUG
        Serial.println("Init BMP180\n");
    #endif
     if (!bmp_in.begin()) {
        Serial.println("BMP180 init fail\n\n");
        oled.print("BMP180 init fail !");
        while (1); // Pause forever.
    }



    //Init MCP
    #ifdef DEBUG
        Serial.println("Init mcp_rh_0\n");
    #endif
    mcp_rh_0.init();
    #ifdef DEBUG
        Serial.println("Init mcp_rh_1\n");
    #endif
    mcp_rh_1.init();
    #ifdef DEBUG
        Serial.println("Init mcp_lh_0\n");
    #endif
    mcp_lh_0.init();
    #ifdef DEBUG
        Serial.println("Init mcp_lh_1\n");
    #endif
    mcp_lh_1.init();

    //Configure MCPs to all input PULLUP
    mcp_rh_0.portMode(MCP23017Port::A, 0xFF, 0xFF); //Input PULLUP
    mcp_rh_0.portMode(MCP23017Port::B, 0xFF, 0xFF); //Input PULLUP
    mcp_rh_1.portMode(MCP23017Port::A, 0xFF, 0xFF); //Input PULLUP
    mcp_rh_1.portMode(MCP23017Port::B, 0xFF, 0xFF); //Input PULLUP
    mcp_lh_0.portMode(MCP23017Port::A, 0xFF, 0xFF); //Input PULLUP
    mcp_lh_0.portMode(MCP23017Port::B, 0xFF, 0xFF); //Input PULLUP
    mcp_lh_1.portMode(MCP23017Port::A, 0xFF, 0xFF); //Input PULLUP
    mcp_lh_1.portMode(MCP23017Port::B, 0xFF, 0xFF); //Input PULLUP
    //Dummy read to point to register
    mcp_rh_0.readPort(MCP23017Port::B);
    mcp_rh_1.readPort(MCP23017Port::B);
    mcp_lh_0.readPort(MCP23017Port::B);
    mcp_lh_1.readPort(MCP23017Port::A);


    //Configure GPIO to INPUT
    pinMode(KEY_MENU, INPUT_PULLUP);
    pinMode(KEY_MISC, INPUT_PULLUP);

    //init some variable
    memset(R_prev_press, 0, sizeof(R_prev_press));
    memset(L_prev_press, 0, sizeof(L_prev_press));
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
    oled.print("INIT OK !");
    delay(1000);
    oled.clear();
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
    memcpy(L_prev_press, L_press, sizeof(L_press));
    //Zero some variables in doubt
    notes_to_play_r.clear();
    notes_to_remove_r.clear();
    notes_to_play_l.clear();
    notes_to_remove_l.clear();
    memset(R_press, 0, sizeof(R_press));
    memset(L_press, 0, sizeof(L_press));

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
        if (p_offset > max_pressure) {p_offset  = max_pressure;}
        if (p_offset < -max_pressure) {p_offset = -max_pressure;}
        // If no pressure, no volume
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
            volume=compute_volume(p_offset);
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
    //Use burst instead of dedicated read to increase acquisition speed
    keys_rh =   (uint32_t)read_burst16_mcp(MCP23017_RH_0_SUB_ADDRESS)&0xffff;
    keys_rh |= ((uint32_t)read_burst16_mcp(MCP23017_RH_1_SUB_ADDRESS)&0xffff)<<16;

    keys_lh =   (uint32_t)read_burst16_mcp(MCP23017_LH_0_SUB_ADDRESS)&0xffff;
    keys_lh |= ((uint32_t)read_burst16_mcp(MCP23017_LH_1_SUB_ADDRESS)&0xffff)<<16;

    //Conversion to rows, then to R_press (bool)
    keys_rh_row[0] =  keys_rh >> (10+ROW_NUMBER_R);
    keys_rh_row[1] = (keys_rh >> 10) & 0x7FF;
    keys_rh_row[2] =  keys_rh & 0x3FF;
    for(uint8_t i=0;i<COLUMN_NUMBER_R;i++){
        for(uint8_t j=0;j<ROW_NUMBER_R;j++){
            R_press[i][j]=(keys_rh_row[i] & (1<<(j)))>>(j);
        }
    }

    remap_left_keys(keys_lh, keys_lh_row);
    for(uint8_t i=0;i<COLUMN_NUMBER_L;i++){
        for(uint8_t j=0;j<ROW_NUMBER_L;j++){
            L_press[i][j]=(keys_lh_row[i] & (1<<(j)))>>(j);
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
    //Right hand
    for (size_t i = 0; i < COLUMN_NUMBER_R; i++) {
        for (size_t j = 0; j < ROW_NUMBER_R; j++) {
            //Add and remove note depending on bellow direction and previous direction
            if (bellow_prev == PUSH && bellow == PULL) {
                if (R_press[i][j]) {
                    notes_to_remove_r.push_back(R_notesP[i][j]);
                    notes_to_play_r.push_back(R_notesT[i][j]);
                }
            } else if (bellow_prev == PULL && bellow == PUSH) {
                if (R_press[i][j]) {
                    notes_to_remove_r.push_back(R_notesT[i][j]);
                    notes_to_play_r.push_back(R_notesP[i][j]);
                }
            }
            if (R_prev_press[i][j]) {
                if (!R_press[i][j]) { //remove note if touch isn't pressed anymore
                    if (bellow_not_null == PUSH || bellow_prev == PUSH)
                    {
                        notes_to_remove_r.push_back(R_notesP[i][j]);
                    }
                    if (bellow_not_null == PULL || bellow_prev == PULL)
                    {
                        notes_to_remove_r.push_back(R_notesT[i][j]);
                    }
                }
            } else if (!R_prev_press[i][j]) { //Add notes
                if (R_press[i][j]) {
                    if (bellow_not_null == PUSH) {
                        notes_to_play_r.push_back(R_notesP[i][j]);
                    }
                    else {
                        notes_to_play_r.push_back(R_notesT[i][j]);
                    }
                }
            }
        }
    }

    //Left hand
    for (size_t i = 0; i < COLUMN_NUMBER_L; i++) {
        for (size_t j = 0; j < ROW_NUMBER_L; j++) {
            //Restart notes if bellow change direction
            if (bellow_prev != bellow) {
                if (L_press[i][j]) {
                    notes_to_remove_l.push_back(L_notes[i][j]);
                    notes_to_play_l.push_back(L_notes[i][j]);
                    if (fifth_enable) {
                        notes_to_remove_l.push_back(L_notes_fifth[i][j]);
                        notes_to_play_l.push_back(L_notes_fifth[i][j]);
                    }
                }
            }
            //Add and remove note depending on bellow direction and previous direction
            if (L_prev_press[i][j]) {
                if (!L_press[i][j]) { //remove note if touch isn't pressed anymore
                    notes_to_remove_l.push_back(L_notes[i][j]);
                    if (fifth_enable) {
                        notes_to_remove_l.push_back(L_notes_fifth[i][j]);
                    }
                }
            } else if (!L_prev_press[i][j]) { //Add notes
                if (L_press[i][j]) {
                    notes_to_play_l.push_back(L_notes[i][j]);
                    if (fifth_enable) {
                        notes_to_play_l.push_back(L_notes_fifth[i][j]);
                    }
                }
            }
        }
    }


    //Uniquify the list to minimize number of message
    notes_to_play_r.sort();
    notes_to_play_r.unique();
    notes_to_remove_r.sort();
    notes_to_remove_r.unique();
    notes_to_play_l.sort();
    notes_to_play_l.unique();
    notes_to_remove_l.sort();
    notes_to_remove_l.unique();
    //-----------------------------------
    // Send MIDI message
    //-----------------------------------
    if(reverse_expr_volume){
        volume_resolved=expression;
        expression_resolved=volume;
    } else {
        volume_resolved=volume;
        expression_resolved=expression;
    }

    //Right hand on channel 1
    while (!notes_to_remove_r.empty()) {
        #ifndef DEBUG
            MIDIUSB.sendNoteOff(notes_to_remove_r.back()+12*octave, 0, 1);
        #endif
        MIDIPI.sendNoteOff(notes_to_remove_r.back()+12*octave, 0, 1);

        notes_to_remove_r.pop_back();
    }
    while (!notes_to_play_r.empty()) {
        #ifndef DEBUG
            MIDIUSB.sendNoteOn(notes_to_play_r.back()+12*octave, expression_resolved, 1);
        #endif
        MIDIPI.sendNoteOn(notes_to_play_r.back()+12*octave, expression_resolved, 1);
        notes_to_play_r.pop_back();
    }

    //Left hand on channel 2
    while (!notes_to_remove_l.empty()) {
        #ifndef DEBUG
            MIDIUSB.sendNoteOff(notes_to_remove_l.back(), 0, 2);
        #endif
        MIDIPI.sendNoteOff(notes_to_remove_l.back(), 0, 2);

        notes_to_remove_l.pop_back();
    }
    while (!notes_to_play_l.empty()) {
        #ifndef DEBUG
            MIDIUSB.sendNoteOn(notes_to_play_l.back(), expression_resolved, 2);
        #endif
        MIDIPI.sendNoteOn(notes_to_play_l.back(), expression_resolved, 2);
        notes_to_play_l.pop_back();
    }

    //Send Volume
    if (bellow != NOPUSH) {bellow_prev = bellow;}
    if(volume_prev!=volume_resolved){
        #ifndef DEBUG
            MIDIUSB.sendControlChange(7,volume_resolved, 1);
            MIDIUSB.sendControlChange(7,volume_resolved, 2);
        #endif
        MIDIPI.sendControlChange(7,volume_resolved, 1);
        MIDIPI.sendControlChange(7,volume_resolved, 2);
    }
    volume_prev=volume_resolved;

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
        for (uint8_t i = 0; i < 128/fontW ; i++) {
            str_oled[i] = '\0';
        }
        //Keyboard left hand
        for (uint8_t i = 0; i < ROW_NUMBER_L; i++) {
            oled.setCursor((128-(COLUMN_NUMBER_L+1)*fontW), 64/fontH-7+i);
            for (uint8_t j = 0;  j < COLUMN_NUMBER_L; j++) {
                if(L_press[j][i]){
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
