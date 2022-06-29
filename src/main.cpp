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

//Matrix definition
#define ROW_NUMBER_R 11
#define COLUMN_NUMBER_R 3

//Useful flags
#define DEBUG   1
#define OCTAVED 0

//Matrix-->arduino pin correspondance
uint8_t rx_pin[] = {28, 26, 24};
uint8_t ry_pin[] = {25, 27, 29, 31, 35, 37, 39, 41, 43, 45, 47};

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

//Pressure stuff
int volume;
int state, state_2; //Contains info if current bellow direction is push or pull
int prev_state;
SFE_BMP180 pressure;
double p_tare;
double p_offset;
long t_start;
float min_pressure = 0.2;
float max_pressure = 15;

//Midi creation
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDI);

//Initial setup
void setup()
{
    //pressure stuff
    prev_state = 1;
    p_tare = 0;
    if (pressure.begin()) {}
    else
    {
        Serial.println("BMP180 init fail\n\n");
        while (1); // Pause forever.
    }

    //midi and IO
    MIDI.begin(1);
    Serial.begin(115200);
    if (DEBUG) {SerialUSB.println("Initializing...");}
    for (size_t i = 0; i < COLUMN_NUMBER_R; i++) {
        pinMode(rx_pin[i], OUTPUT);
        digitalWrite(rx_pin[i], HIGH);
    }
    for (size_t i = 0; i < ROW_NUMBER_R; i++) {
        pinMode(ry_pin[i], INPUT);
        digitalWrite(ry_pin[i], HIGH);
    }
    memset(R_prev_press, 0, sizeof(R_prev_press));
    if (DEBUG) {SerialUSB.println("End INIT...");}
}

//Main loop
void loop()
{
    //-----------------------------------
    // local variables
    //-----------------------------------
    //Temperature and Pressure
    double T, P;
    //Status for the pressure sensor
    char status;

    //-----------------------------------
    // Initialise loop
    //-----------------------------------
    //remember old pressed touch
    memcpy(R_prev_press, R_press, sizeof(R_press));
    memset(R_press, 0, sizeof(R_press));

    //-----------------------------------
    // Temp & pressure mesurement part 1
    //-----------------------------------
    // Start temp mesurement
    status = pressure.startTemperature();
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}
    // Wait for the measurement to complete:
    delay(status);

    // Retrieve the completed temperature measurement:
    status = pressure.getTemperature(T);
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}

    // Start a pressure measurement:
    status = pressure.startPressure(3);
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}

    t_start = millis();

    //-----------------------------------
    // Matrix key acquisition
    //-----------------------------------
    for (size_t i = 0; i < COLUMN_NUMBER_R; i++) {
        digitalWrite(rx_pin[i], LOW);
        for (size_t j = 0; j < ROW_NUMBER_R; j++)
        {
            if (digitalRead(ry_pin[j]) == LOW) {R_press[i][j] = 0;}
            else {R_press[i][j] = 1;}
        }
        digitalWrite(rx_pin[i], HIGH);
    }


    //-----------------------------------
    // Pressure mesurement part 2 & calc
    //-----------------------------------
    while (millis() - t_start < status);
    status = pressure.getPressure(P, T);
    if (status == 0) {Serial.println("error retrieving pressure measurement\n");}

    //Get the pressure at initial time (if not defined)
    if (p_tare == 0) {p_tare = P;}
    p_offset = P - p_tare;
    //Trunk the pressure if too big/low
    if (p_offset > max_pressure) {p_offset = max_pressure;}
    if (p_offset < -max_pressure) {p_offset = -max_pressure;}
    //If no pressure, no volume
    if (abs(p_offset) < min_pressure) {
        state = 0;
        volume = 0;
    } else {
        if (p_offset < 0) { //PULL
            state = -1;
            state_2 = -1;
        } else {            //PUSH
            state = 1;
            state_2 = 1;
        }
        //volume calculation :
        p_offset = abs(p_offset);
        volume = int((log(float(p_offset) / 55) + 6) * 27) ;
    }


    //-----------------------------------
    // Prepare MIDI message
    //-----------------------------------
    for (size_t i = 0; i < COLUMN_NUMBER_R; i++) {
        for (size_t j = 0; j < ROW_NUMBER_R; j++) {
            if (prev_state == 1 && state == -1) {
                if (R_press[i][j]) {
                    notes_to_remove.push_back(R_notesP[i][j]);
                    notes_to_play.push_back(R_notesT[i][j]);
                }
            } else if (prev_state == -1 && state == 1) {
                if (R_press[i][j]) {
                    notes_to_remove.push_back(R_notesT[i][j]);
                    notes_to_play.push_back(R_notesP[i][j]);
                }
            }
            if (R_prev_press[i][j]) {
                if (!R_press[i][j]) { //remove note if touch isn't pressed anymore
                    if (state_2 == 1 || prev_state == 1)
                    {
                        notes_to_remove.push_back(R_notesP[i][j]);
                    }
                    if (state_2 == -1 || prev_state == -1)
                    {
                        notes_to_remove.push_back(R_notesT[i][j]);
                    }
                }
            } else if (!R_prev_press[i][j]) {
                if (R_press[i][j]) {
                    if (state_2 == 1) {
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
            MIDI.sendNoteOff(notes_to_remove.back()+12*OCTAVED, 0, 1);
        }

        notes_to_remove.pop_back();
    }
    while (!notes_to_play.empty()) {
        //Serial.println(notes_to_play.size(),10);
        //Serial.println(notes_to_play.empty());
        //Serial.println(notes_to_play.back(),10);
        if (!DEBUG)
        {
        MIDI.sendNoteOn(notes_to_play.back()+12*OCTAVED, 100, 1);
        }
        notes_to_play.pop_back();
    }
    if (state != 0) {prev_state = state;}

  //delay(500);
  
    if (!DEBUG)
    {
        MIDI.sendControlChange(7,volume, 1);
        // MIDI.sendControlChange(7,100, 1);
    }


    //-----------------------------------
    // DEBUG
    //-----------------------------------
    //If debug, print the current detected pressed touch on terminal with X, other with O
    if (DEBUG)
    {
        Serial.write(27);       // ESC command
        Serial.print("[2J");    // clear screen command
        Serial.write(27);
        Serial.print("[H");     // cursor to home command
        for (size_t j = 0; j < ROW_NUMBER_R; j++) {
            for (size_t i = 0; i < COLUMN_NUMBER_R; i++) {
                if(R_press[i][j]) {
                    Serial.print("X");
                } else {
                    Serial.print("O");
                }
            }
            Serial.print("\n\r");
        }
        delay(100);
    }
}
