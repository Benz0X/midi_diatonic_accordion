/* Diatonic accordion midi project */
//TODO : change key->midi handling (if a key should be played, add 1 to it's value and play it if the value is >0)
//TODO : do midi add and key detect in the same loop (add delay, may even be good)

#include "Arduino.h"

//Ugly hack to solve issue with functions defined twice
#undef max
#undef min

//Libs & header import
#include <MIDI.h>
#include "midinote.h"
#include <SFE_BMP180.h>
#include <Wire.h>
#include <list>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_BMP085.h>
//Matrix definition
#define ROW_NUMBER_R 11
#define COLUMN_NUMBER_R 3

//Useful define
#define PUSH 1
#define NOPUSH 2
#define PULL 0

//Useful flags
#define DEBUG   0
#define OCTAVED 1

#define MIDI_GPIO


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
char status;

//Debug stuff
byte data[5];
char buffer[256];
uint32_t buffpointer;


//Midi creation
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDIUSB);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDIPI);

//Initial setup
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
    if (DEBUG) {Serial.println("Init BMP180\n");}
     if (!bmp_in.begin()) {
        Serial.println("BMP180 init fail\n\n");
        while (1); // Pause forever.
    }

    //Init MCP
    if (DEBUG) {Serial.println("Init mcp_md_0\n");}
    if (!mcp_md_0.begin_I2C()) {
        Serial.println("Error mcp_md_0.");
        while (1);
    }
    if (DEBUG) {Serial.println("Init mcp_md_1\n");}
    if (!mcp_md_1.begin_I2C(0x24)) {
        Serial.println("Error mcp_md_1.");
        while (1);
    }


    //Configure MCPs to all input PULLUP
    for(int i=0;i<16;i++){
        mcp_md_0.pinMode(i, INPUT_PULLUP);
        mcp_md_1.pinMode(i, INPUT_PULLUP);
    }

    //init some variable
    memset(R_prev_press, 0, sizeof(R_prev_press));
    volume_prev=0;
    bellow_prev=NOPUSH;

    //Mesure initial pressure at boot
    p_tare=0;
    status = bmp_in.startTemperature();
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}
    delay(status);
    status = bmp_in.getTemperature(T);
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}

    // Start a pressure measurement:
    status = bmp_in.startPressure(3);
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}

    t_start = millis();
    while (millis() - t_start < status);
    status = bmp_in.getPressure(P, T);
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}
    p_tare=P;


    data[0]=0xf0;
    data[1]=0x7d;
    data[2]=0x03;
    data[3]=0x01;
    data[4]=0xf7;
        MIDIPI.sendControlChange(7,70, 1);

        MIDIPI.sendSysEx(5, data, 1);

    if (DEBUG) {Serial.println("INIT OK.");}
}

//Main loop
void loop()
{

    //-----------------------------------
    // Initialise loop
    //-----------------------------------
    buffpointer=0;
    //remember old pressed touch
    memcpy(R_prev_press, R_press, sizeof(R_press));
    memset(R_press, 0, sizeof(R_press));

    //-----------------------------------
    // Temp & pressure mesurement part 1
    //-----------------------------------
    // Start temp mesurement
    status = bmp_in.startTemperature();
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}
    // Wait for the measurement to complete:
    delay(status);

    // Retrieve the completed temperature measurement:
    status = bmp_in.getTemperature(T);
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}

    // Start a pressure measurement:
    status = bmp_in.startPressure(3);
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}

    t_start = millis();


    //-----------------------------------
    // Matrix key acquisition
    //-----------------------------------
    keys_md = (uint32_t) mcp_md_0.readGPIO(1) & 0xff;
    keys_md |= ((uint32_t) mcp_md_0.readGPIO(0) & 0xff)<<8;
    keys_md |= ((uint32_t) mcp_md_1.readGPIO(1) & 0xff)<<16;
    keys_md |= ((uint32_t) mcp_md_1.readGPIO(0) & 0xff)<<24;

    //Conversion to R_press
    keys_md_row[0] = keys_md >> (10+ROW_NUMBER_R);
    keys_md_row[1] = (keys_md >> 10) & 0x7FF;
    keys_md_row[2] = keys_md & 0x3FF;
    for(uint8_t i=0;i<COLUMN_NUMBER_R;i++){
        for(uint8_t j=0;j<ROW_NUMBER_R;j++){
            R_press[i][j]=(keys_md_row[i] & (1<<(j)))>>(j);
        }
    }

    //-----------------------------------
    // Pressure mesurement part 2 & calc
    //-----------------------------------
    while (millis() - t_start < status);
    status = bmp_in.getPressure(P, T);
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}

    //Get the pressure at initial time (if not defined)
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
            bellow_not_null = PULL;
        } else {            //PUSH
            bellow          = PUSH;
            bellow_not_null = PUSH;
        }
        //volume calculation :
        p_offset = abs(p_offset);
        volume = uint8_t((log(float(p_offset) / 55) + 6) * 27) ;
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

    //-----------------------------------
    // Send MIDI message
    //-----------------------------------
    while (!notes_to_remove.empty()) {
        //Serial.println(notes_to_remove.size(),10);
        //Serial.println(notes_to_remove.empty());
        //Serial.println(notes_to_remove.back(),10);
        if (!DEBUG)
        {
            MIDIUSB.sendNoteOff(notes_to_remove.back()+12*OCTAVED, 0, 1);
            MIDIPI.sendNoteOff(notes_to_remove.back()+12*OCTAVED, 0, 1);
        }

        notes_to_remove.pop_back();
    }
    while (!notes_to_play.empty()) {
        //Serial.println(notes_to_play.size(),10);
        //Serial.println(notes_to_play.empty());
        //Serial.println(notes_to_play.back(),10);
        if (!DEBUG)
        {
        MIDIUSB.sendNoteOn(notes_to_play.back()+12*OCTAVED, 100, 1);
        MIDIPI.sendNoteOn(notes_to_play.back()+12*OCTAVED, 100, 1);
        }
        notes_to_play.pop_back();
    }
    if (bellow != NOPUSH) {bellow_prev = bellow;}

  
    if (!DEBUG)
    {
        if(volume_prev!=volume){
            MIDIUSB.sendControlChange(7,volume, 1);
            MIDIPI.sendControlChange(7,volume, 1);
        }
    }
    volume_prev=volume;


    if (DEBUG)
        {
            // Serial.write(27);       // ESC command
            // Serial.print("[2J");    // clear screen command
            // Serial.write(27);
            // Serial.print("[H");     // cursor to home command
            for (size_t j = 0; j < COLUMN_NUMBER_R; j++) {
                for (size_t i = 0; i < ROW_NUMBER_R; i++) {
                    if(R_press[j][i]) {
                        Serial.print("X");
                    } else {
                        Serial.print("-");
                    }
                }
                Serial.print("   ");
            }
            Serial.print("\n\r");
            delay(100);
        }

}
