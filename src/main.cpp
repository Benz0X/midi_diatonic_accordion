/* Diatonic accordion midi project */
//TODO : refactor code to use multiple files
//TODO : refactor code to enable/disable functionnalities
// Use midinote struct & fonctions ?
//TODO : create preset for usecase (fully neutral, balanced output for Reaper/soundcard, preset for 'hit' instruments, preset for MT32 with correct channels, preset for headset)
//TODO : Adjust volume depending on number of voices on RH
//TODO : try to compress the code by using NoteOn with zero velocity instead of NoteOff (this suppose MIDI Running Status is used)
//TODO : some glitch still appear when doing octaves on RH with bassoon or picolo are enabled with flute.
//             Option 1 : treat bassoon and piccolo with different channels,
//             Option 2 : use a separate R_played_notes/prev: one for vibrato and flute/bassoon/picolo

#include "Arduino.h"

//Ugly hack to solve issue with functions defined twice
#undef max
#undef min

//This allow to switch between darwin style left hand (24 buttons, same sound on pull/pull) and traditionnal 18 basses layout
#define DARWIN

// Use to send some info on UART, this will disable MIDI over USB (as it is used for UART)
// #define DEBUG

// This disable sending MIDI message over USB (messages will only be sent to the serial out to the RPI) This speed up a little the loop
#define DISABLE_MIDI_USB


#ifdef DEBUG
#define DISABLE_MIDI_USB
#endif

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

//Contains notes to play, this use a bit more memory than using lists with not to play/remove but Arduino Due can afford it and it prevent glitchs when same note is played several time
uint8_t R_played_note[mid_B9];      //RH won't go higher than B9
uint8_t R_played_note_prev[mid_B9]; //RH won't go higher than B9

uint8_t L_played_note[mid_B6];      //LH won't go higher then B6
uint8_t L_played_note_prev[mid_B6]; //LH won't go higher then B6


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
    //    {mid_F3+1+4, mid_A3+4, mid_C4+4, mid_E4+4, mid_F4+1+4, mid_A4+4, mid_C5+4, mid_E5+4, mid_F5+1+4, mid_G5+4, mid_C6+4},
    //    {mid_G3, mid_B3, mid_D4, mid_F4, mid_A4, mid_B4, mid_D5, mid_F5, mid_A5, mid_B5, mid_D6},
    //    {mid_B3-1, mid_C4+1, mid_G4, mid_A4-1, mid_B4-1, mid_C5+1, mid_E5-1, mid_A5-1, mid_B5-1, mid_C6+1, mid_E6-1}};
    // uint8_t R_notesP[COLUMN_NUMBER_R][ROW_NUMBER_R] = {
    //    {mid_D3+4, mid_G3+4, mid_B3+4, mid_D4+4, mid_G4+4, mid_B4+4, mid_D5+4, mid_G5+4, mid_B5+4, mid_D6+4, mid_G6+4},
    //    {mid_E3, mid_G3, mid_C4, mid_E4, mid_G4, mid_C5, mid_E5, mid_G5, mid_C6, mid_E6, mid_G6},
    //    {mid_A3-1, mid_B3-1, mid_E4-1, mid_A4-1, mid_B4-1, mid_E5-1, mid_A5-1, mid_B5-1, mid_E6-1, mid_A6-1, mid_B6-1}};


//Notes definitions for left hand (no push/pull distinction as the keyboard layout is a Serafini Darwin whith same sound in push and pull)
#ifdef DARWIN
uint8_t L_notesT[COLUMN_NUMBER_L][ROW_NUMBER_L] = {
        {mid_D3+1, mid_F3  , mid_G3  , mid_A3  , mid_B3  , mid_C3+1 }, //1rst row
        {mid_A3+1, mid_C3  , mid_D3  , mid_E3  , mid_F3+1, mid_G3+1 }, //2nd row
        {mid_D4+1, mid_F4  , mid_G4  , mid_A4  , mid_B4  , mid_C4+1 }, //3nd row
        {mid_A4+1, mid_C4  , mid_D4  , mid_E4  , mid_F4+1, mid_G4+1 }};//4rd row
//Fifth definitions for left hand (so we can build our own fifth chords)
uint8_t L_notes_fifthT[COLUMN_NUMBER_L][ROW_NUMBER_L] = {
        {0       , 0       , 0       , 0       , 0       , 0        }, //1rst row
        {0       , 0       , 0       , 0       , 0       , 0        }, //2nd row
        {mid_A4+1, mid_C4  , mid_D4  , mid_E4  , mid_F4+1, mid_G4+1 }, //3nd row
        {mid_F4  , mid_G4  , mid_A4  , mid_B4  , mid_C4+1, mid_D4+1 }};//4rd row
//Ugly way to assign P to T as push pull is the same in darwin, this save a little memory
uint8_t (*L_notesP)[ROW_NUMBER_L]       = L_notesT;
uint8_t (*L_notes_fifthP)[ROW_NUMBER_L] = L_notes_fifthT;
#else
uint8_t L_notesT[COLUMN_NUMBER_L][ROW_NUMBER_L] = {
        {0       , 0       , 0       , 0       , 0       , 0        }, //1rst row
        {mid_C3  , mid_E3  , mid_G3+1, mid_F3+1, mid_C3+1, mid_D3+1 }, //2nd row
        {mid_F3  , mid_F4  , mid_A3  , mid_A4  , mid_A3+1, mid_A4+1 }, //3nd row
        {mid_G3  , mid_G4  , mid_D3  , mid_D4  , mid_B3  , mid_B4   }};//4rd row
//Fifth definitions for left hand (so we can build our own fifth chords)
uint8_t L_notes_fifthT[COLUMN_NUMBER_L][ROW_NUMBER_L] = {
        {0       , 0       , 0       , 0       , 0       , 0        }, //1rst row
        {0       , 0       , 0       , 0       , 0       , 0        }, //2nd row
        {0       , mid_C4  , 0       , mid_E4  , 0       , mid_F4   }, //3nd row
        {0       , mid_D4  , 0       , mid_A4  , 0       , mid_F4+1 }};//4rd row
uint8_t L_notesP[COLUMN_NUMBER_L][ROW_NUMBER_L] = {
        {0       , 0       , 0       , 0       , 0       , 0        }, //1rst row
        {mid_D3  , mid_A3  , mid_B3  , mid_F3+1, mid_C3+1, mid_A3+1 }, //2nd row
        {mid_F3  , mid_F4  , mid_E3  , mid_E4  , mid_D3+1, mid_D4+1 }, //3nd row
        {mid_C3  , mid_C4  , mid_G3  , mid_G4  , mid_G3+1, mid_G4+1 }};//4rd row
//Fifth definitions for left hand (so we can build our own fifth chords)
uint8_t L_notes_fifthP[COLUMN_NUMBER_L][ROW_NUMBER_L] = {
        {0       , 0       , 0       , 0       , 0       , 0        }, //1rst row
        {0       , 0       , 0       , 0       , 0       , 0        }, //2nd row
        {0       , mid_C4  , 0       , mid_B4  , 0       , mid_A4+1 }, //3nd row
        {0       , mid_G4  , 0       , mid_D4  , 0       , mid_D4+1 }};//4rd row

#endif

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
float min_pressure = 0.07;
float max_pressure = 13;
char bmp_status;
bool waiting_t=0;
bool waiting_p=0;


//Midi creation
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDIUSB);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDIPI);
//Helper functions
void midi_broadcast_control_change(uint8_t cc, uint8_t value, uint8_t channel) {
    #ifndef DISABLE_MIDI_USB
        MIDIUSB.sendControlChange(cc, value, channel);
    #endif
    MIDIPI.sendControlChange(cc, value, channel);
}

void midi_broadcast_program_change(uint8_t program, uint8_t channel) {
    #ifndef DISABLE_MIDI_USB
        MIDIUSB.sendProgramChange(program, channel);
    #endif
    MIDIPI.sendProgramChange(program, channel);
}
void midi_broadcast_note_on(uint8_t note, uint8_t expression, uint8_t channel) {
    #ifndef DISABLE_MIDI_USB
        MIDIUSB.sendNoteOn(note, expression, channel);
    #endif
    MIDIPI.sendNoteOn(note, expression, channel);
}
void midi_broadcast_note_off(uint8_t note, uint8_t expression, uint8_t channel) {
    #ifndef DISABLE_MIDI_USB
        MIDIUSB.sendNoteOff(note, expression, channel);
    #endif
    MIDIPI.sendNoteOff(note, expression, channel);
}
void midi_broadcast_send(midi::MidiType command, uint8_t msb, uint8_t lsb, uint8_t channel) {
    #ifndef DISABLE_MIDI_USB
        MIDIUSB.send(command, msb, lsb, channel);
    #endif
    MIDIPI.send(command, msb, lsb, channel);
}
void midi_broadcast_pitchbend(int pitchvalue, uint8_t channel) {
    #ifndef DISABLE_MIDI_USB
        MIDIUSB.sendPitchBend(pitchvalue, channel);
    #endif
    MIDIPI.sendPitchBend(pitchvalue, channel);
}

//-----------------------------------
//Menu definition
//-----------------------------------
uint8_t volume_attenuation  = 48;
uint8_t expression          = 127;
uint8_t mt32_rom_set        = 0;
uint8_t mt32_soundfont      = 1;
int8_t  octave              = 1;
uint8_t pressuremode        = 2;
uint8_t program_rh          = 0;
uint8_t program_lh          = 1;
uint8_t channel_rh          = 1;
uint8_t channel_lh          = 2;
uint8_t pano_rh             = 38;
uint8_t pano_lh             = 42;
uint8_t reverb_rh           = 8;
uint8_t reverb_lh           = 8;
uint8_t chorus_rh           = 4;
uint8_t chorus_lh           = 0;
bool    mt32_synth          = MT32_SOUNDFONT;
bool    debug_oled          = 0;
bool    dummy               = 0;
bool    reverse_expr_volume = 0;
bool    fifth_enable        = 1;
bool    bassoon_enable      = 0;
bool    picolo_enable       = 0;
bool    flute_enable        = 1;
int8_t  vibrato             = 0;
int8_t  vibrato_prev        = 0;
int8_t  transpose           = 0;
#define VIBRATO_CHANNEL       8

result menu_midi_pano_change_rh() {
    midi_broadcast_control_change(MIDI_CC_BALANCE,pano_rh, channel_rh);
    return proceed;
}
result menu_midi_pano_change_lh() {
    midi_broadcast_control_change(MIDI_CC_BALANCE,pano_lh, channel_lh);
    return proceed;
}
result menu_midi_reverb_change_rh() {
    midi_broadcast_control_change(MIDI_CC_REVERB,reverb_rh, channel_rh);
    return proceed;
}
result menu_midi_reverb_change_lh() {
    midi_broadcast_control_change(MIDI_CC_REVERB,reverb_lh, channel_lh);
    return proceed;
}
result menu_midi_chorus_change_rh() {
    midi_broadcast_control_change(MIDI_CC_CHORUS,chorus_rh, channel_rh);
    return proceed;
}
result menu_midi_chorus_change_lh() {
    midi_broadcast_control_change(MIDI_CC_CHORUS,chorus_lh, channel_lh);
    return proceed;
}
result menu_midi_program_change_rh() {
    midi_broadcast_program_change(program_rh, channel_rh);
    menu_midi_pano_change_rh();
    menu_midi_reverb_change_rh();
    menu_midi_chorus_change_rh();
    midi_broadcast_control_change(MIDI_CC_VOLUME, 0, channel_rh);
    return proceed;
}
result menu_midi_program_change_lh() {
    midi_broadcast_program_change(program_lh, channel_lh);
    menu_midi_pano_change_lh();
    menu_midi_reverb_change_lh();
    menu_midi_chorus_change_lh();
    midi_broadcast_control_change(MIDI_CC_VOLUME, 0, channel_lh);
    return proceed;
}

result menu_midi_vibrato_pitch() {
    midi_broadcast_program_change(program_rh, VIBRATO_CHANNEL);
    // midi_broadcast_control_change(MIDI_CC_MODWHEEL, vibrato, VIBRATO_CHANNEL);
    midi_broadcast_pitchbend(vibrato*64, VIBRATO_CHANNEL); //This is way better than modwheel

    //Disable chorus
    chorus_rh=0;
    menu_midi_chorus_change_rh();
    //Set balance for vibrato and silence it
    midi_broadcast_control_change(MIDI_CC_BALANCE,pano_rh, VIBRATO_CHANNEL);
    midi_broadcast_control_change(MIDI_CC_VOLUME, 0, VIBRATO_CHANNEL);
    return proceed;
}

result menu_mt32_switch_rom_set() {
    mt32_switch_rom_set(mt32_rom_set, MIDIPI);
    return proceed;
}
result menu_mt32_switch_soundfont() {
    mt32_switch_soundfont(mt32_soundfont, MIDIPI);
    menu_midi_program_change_rh();
    menu_midi_program_change_lh();
    return proceed;
}
result menu_mt32_switch_synth() {
    mt32_switch_synth(mt32_synth, MIDIPI);
    menu_midi_program_change_rh();
    menu_midi_program_change_lh();
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

//MIDICONF submenu
TOGGLE(reverse_expr_volume, reversectrl, "Velo/expr : ", doNothing, noEvent, noStyle
       , VALUE("NORMAL", LOW, menu_mt32_switch_synth, noEvent)
       , VALUE("INVERTED", HIGH, menu_mt32_switch_synth, noEvent)
      );
MENU(midi_config, "MIDI config", doNothing, noEvent, wrapStyle
     , FIELD(channel_lh, "Channel LH :", "", 0, 15, 1, 1  , doNothing                  , anyEvent, wrapStyle)
     , FIELD(channel_rh, "Channel RH :", "", 0, 15, 1, 1  , doNothing                  , anyEvent, wrapStyle)
     , FIELD(program_lh, "Program LH :", "", 0, 128, 16, 1, menu_midi_program_change_lh, anyEvent, wrapStyle)
     , FIELD(program_rh, "Program RH :", "", 0, 128, 16, 1, menu_midi_program_change_rh, anyEvent, wrapStyle)
     , FIELD(pano_lh   , "Pano LH :"   , "", 0, 127, 16, 1, menu_midi_pano_change_lh   , anyEvent, wrapStyle)
     , FIELD(pano_rh   , "Pano RH :"   , "", 0, 127, 16, 1, menu_midi_pano_change_rh   , anyEvent, wrapStyle)
     , FIELD(reverb_lh , "Reverb LH :" , "", 0, 127, 16, 1, menu_midi_reverb_change_lh , anyEvent, wrapStyle)
     , FIELD(reverb_rh , "Reverb RH :" , "", 0, 127, 16, 1, menu_midi_reverb_change_rh , anyEvent, wrapStyle)
     , FIELD(chorus_lh , "Chorus LH :" , "", 0, 127, 16, 1, menu_midi_chorus_change_lh , anyEvent, wrapStyle)
     , FIELD(chorus_rh , "Chorus RH :" , "", 0, 127, 16, 1, menu_midi_chorus_change_rh , anyEvent, wrapStyle)
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
TOGGLE(octave, octavectrl,          "Octaved : ", doNothing, noEvent, noStyle
       , VALUE("-2", -2, doNothing, noEvent)
       , VALUE("-1", -1, doNothing, noEvent)
       , VALUE("2", 2, doNothing, noEvent)
       , VALUE("1", 1, doNothing, noEvent)
       , VALUE("0", 0, doNothing, noEvent)
      );
TOGGLE(fifth_enable, fifthctrl,     "Fifth   : ", doNothing, noEvent, noStyle
       , VALUE("ON", HIGH, doNothing, noEvent)
       , VALUE("OFF", LOW, doNothing, noEvent)
      );
TOGGLE(vibrato, pitchctrl,          "Pitch   : ", doNothing, noEvent, noStyle
       , VALUE("8", 8, menu_midi_vibrato_pitch, noEvent)
       , VALUE("4", 4, menu_midi_vibrato_pitch, noEvent)
       , VALUE("2", 2, menu_midi_vibrato_pitch, noEvent)
       , VALUE("1", 1, menu_midi_vibrato_pitch, noEvent)
       , VALUE("None", 0, menu_midi_vibrato_pitch, noEvent)
      );
TOGGLE(bassoon_enable, bassoonctrl, "Bassoon : ", doNothing, noEvent, noStyle
       , VALUE("ON", HIGH, doNothing, noEvent)
       , VALUE("OFF", LOW, doNothing, noEvent)
      );
TOGGLE(picolo_enable, picoloctrl,   "Picolo  : ", doNothing, noEvent, noStyle
       , VALUE("ON", HIGH, doNothing, noEvent)
       , VALUE("OFF", LOW, doNothing, noEvent)
      );
TOGGLE(flute_enable, flutectrl,     "Flute   : ", doNothing, noEvent, noStyle
       , VALUE("ON", HIGH, doNothing, noEvent)
       , VALUE("OFF", LOW, doNothing, noEvent)
      );
TOGGLE(transpose, transposectrl,    "Key     : ", doNothing, noEvent, noStyle
       , VALUE("G/C",   0,  doNothing, noEvent)
       , VALUE("G#/C#", 1,  doNothing, noEvent)
       , VALUE("A/D",   2,  doNothing, noEvent)
       , VALUE("Bb/Eb", 3,  doNothing, noEvent)
       , VALUE("B/E",   4,  doNothing, noEvent)
       , VALUE("C/F",   5,  doNothing, noEvent)
       , VALUE("C#/F#", 6,  doNothing, noEvent)
       , VALUE("D/G",   7,  doNothing, noEvent)
       , VALUE("Eb/Ab", 8,  doNothing, noEvent)
       , VALUE("E/A",   9,  doNothing, noEvent)
       , VALUE("F/Bb",  10, doNothing, noEvent)
       , VALUE("F#/B",  11, doNothing, noEvent)
      );
MENU(keyboard_config, "Keyboard config", doNothing, noEvent, wrapStyle
     , SUBMENU(octavectrl)
     , SUBMENU(fifthctrl)
     , SUBMENU(bassoonctrl)
     , SUBMENU(flutectrl)
     , SUBMENU(picoloctrl)
     , SUBMENU(pitchctrl)
     , SUBMENU(transposectrl)
    );

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
MENU(mainMenu, "Main menu", doNothing, noEvent, wrapStyle
     , SUBMENU(volmumectrl)
     , SUBMENU(expressionctrl)
     , SUBMENU(mt32_config)
     , SUBMENU(midi_config)
     , SUBMENU(keyboard_config)
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
        // return uint8_t(0.1*pow(x-6,3)+x+79) ;
        return uint8_t(0.1*pow(x-7,3)+2*x+79) ;
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

//This function will ensure basses notes stay in the basses range, and same for chords notes, usefull during transpose
uint8_t transpose_left_hand(uint8_t note_in, uint8_t transpose) {
    if (note_in<mid_C4 && note_in>=mid_C3 ) { //Bass
        return (note_in+transpose)%12+mid_C3;
    } else if(note_in>=mid_C4 && note_in<mid_C5){
        return (note_in+transpose)%12+mid_C4;
    }
    return note_in;
}

//###############################################
//Presets
//###############################################
#define TOGGLE_BASSOON 0
#define TOGGLE_FLUTE   1
#define TOGGLE_PICCOLO 2
#define TOGGLE_VIBRATO 3

void set_preset(uint8_t preset){
    switch (preset) {
        case TOGGLE_BASSOON:
            bassoon_enable=!bassoon_enable;
            break;
        case TOGGLE_FLUTE:
            flute_enable=!flute_enable;
            break;
        case TOGGLE_PICCOLO:
            picolo_enable=!picolo_enable;
            break;
        case TOGGLE_VIBRATO:
            if(vibrato){ //Remove vibrato
                vibrato_prev=vibrato;
                vibrato=0;
            } else if (vibrato==0 && vibrato_prev==0) { //Add vibrato from 0
                vibrato=2;
            } else {
                vibrato=vibrato_prev;
            }
            menu_midi_vibrato_pitch();
            break;
        default:
            break;
    }
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
    Wire.setClock(400000); //500 seems OK, reduce to 400 if artifact

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
    memset(R_played_note,      0, sizeof(R_played_note     ));
    memset(R_played_note_prev, 0, sizeof(R_played_note_prev));
    memset(L_played_note,      0, sizeof(L_played_note     ));
    memset(L_played_note_prev, 0, sizeof(L_played_note_prev));

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

    mt32_switch_synth(mt32_synth, MIDIPI);
    menu_mt32_switch_soundfont();
    menu_midi_program_change_rh();
    menu_midi_program_change_lh();
    menu_midi_pano_change_lh();
    menu_midi_pano_change_rh();
    menu_midi_reverb_change_rh();
    menu_midi_reverb_change_lh();
    menu_midi_chorus_change_rh();
    menu_midi_chorus_change_lh();
    midi_broadcast_control_change(MIDI_CC_VOLUME, 0, channel_lh);
    midi_broadcast_control_change(MIDI_CC_VOLUME, 0, channel_rh);

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
    //remember old pressed touch and notes
    memcpy(R_prev_press, R_press, sizeof(R_press));
    memcpy(L_prev_press, L_press, sizeof(L_press));
    memcpy(R_played_note_prev, R_played_note, sizeof(R_played_note));
    memcpy(L_played_note_prev, L_played_note, sizeof(L_played_note));

    //Reset note count
    memset(R_played_note,      0, sizeof(R_played_note));
    memset(L_played_note,      0, sizeof(L_played_note));
    //Zero some variables in doubt
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
        bmp_status = bmp_in.startPressure(0);
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
    //Menu navigation & presets
    //-----------------------------------
    //For some reason, going low on a FIELD crash the menu, try to avoid it
    if(digitalRead(KEY_MENU)) {
        if (R_press[1][9] && ! R_prev_press[1][9]) {
            strIn.write('+'); //Up
            nav.doInput(strIn);
        }
        if (R_press[1][10] && ! R_prev_press[1][10]) {
            strIn.write('-'); //Down
            nav.doInput(strIn);
        }
        if (R_press[2][9] && ! R_prev_press[2][9]) {
            strIn.write('/'); //Prev
            nav.doInput(strIn);
        }
        if (R_press[0][9] && ! R_prev_press[0][9]) {
            strIn.write('*'); //Next
            nav.doInput(strIn);
        }
        nav.poll();
        //Presets
        if (R_press[2][8] && ! R_prev_press[2][8]) {
            set_preset(TOGGLE_BASSOON);
        }
        if (R_press[2][7] && ! R_prev_press[2][7]) {
            set_preset(TOGGLE_VIBRATO);
        }
        if (R_press[2][6] && ! R_prev_press[2][6]) {
            set_preset(TOGGLE_PICCOLO);
        }
        if (R_press[2][5] && ! R_prev_press[2][5]) {
            set_preset(TOGGLE_FLUTE);
        }

        if(!debug_oled){
            oled.setCursor((128-2*fontW), 0);
            if(bassoon_enable){
                oled.print("B");
            } else {
                oled.print(" ");
            }
            oled.setCursor((128-2*fontW), 1);
            if(vibrato!=0){
                oled.print("V");
            } else {
                oled.print(" ");
            }
            oled.setCursor((128-2*fontW), 2);
            if(picolo_enable){
                oled.print("P");
            } else {
                oled.print(" ");
            }
            oled.setCursor((128-2*fontW), 3);
            if(flute_enable){
                oled.print("F");
            } else {
                oled.print(" ");
            }
        }
    }


    //-----------------------------------
    // Prepare MIDI message
    //-----------------------------------
    if(!digitalRead(KEY_MENU)) {
        //Right hand
        for (size_t i = 0; i < COLUMN_NUMBER_R; i++) {
            for (size_t j = 0; j < ROW_NUMBER_R; j++) {
                if (R_press[i][j]) {
                    if (bellow == PULL) {
                        R_played_note[R_notesT[i][j]+transpose]++;
                    } else if (bellow == PUSH) {
                        R_played_note[R_notesP[i][j]+transpose]++;
                    }
                }
            }
        }

        //Left hand
        for (size_t i = 0; i < COLUMN_NUMBER_L; i++) {
            for (size_t j = 0; j < ROW_NUMBER_L; j++) {
                if (L_press[i][j]) {
                    if (bellow == PULL) {
                        L_played_note[transpose_left_hand(L_notesT[i][j], transpose)]++;
                        if (fifth_enable) {
                            L_played_note[transpose_left_hand(L_notes_fifthT[i][j], transpose)]++;
                        }
                    } else if (bellow == PUSH) {
                        L_played_note[transpose_left_hand(L_notesP[i][j], transpose)]++;
                        if (fifth_enable) {
                            L_played_note[transpose_left_hand(L_notes_fifthP[i][j], transpose)]++;
                        }
                    }
                }
            }
        }
    }

    //-----------------------------------
    // Send MIDI message
    //-----------------------------------
    if(reverse_expr_volume){
        volume_resolved = (expression>volume_attenuation) ? expression-volume_attenuation : 0;
        expression_resolved = (volume != 0) ? volume+volume_attenuation : 0;
    } else {
        volume_resolved=volume;
        expression_resolved=expression;
    }

    //Right hand on channel_rh
    //Remove all notes first, this goes faster than removing/adding if MIDI RunningStatus is enabled (compress messages with same command/channel)
    for (int i = 0; i < sizeof(R_played_note); ++i) {
        if (!R_played_note[i] && R_played_note_prev[i]) {
            if (bassoon_enable) {
                midi_broadcast_note_off(i+12*octave-12, 0, channel_rh);
            }
            if (picolo_enable) {
                midi_broadcast_note_off(i+12*octave+12, 0, channel_rh);
            }
            if (flute_enable) {
                midi_broadcast_note_off(i+12*octave, 0, channel_rh);
            }
        }
    }
    //For the same reason, we do vibrato separatedly even if it duplicates a lot of code
    for (int i = 0; i < sizeof(R_played_note); ++i) {
        if (!R_played_note[i] && R_played_note_prev[i]) {
            if (vibrato!=0) {
                midi_broadcast_note_off(i+12*octave, 0, VIBRATO_CHANNEL);
            }
        }
    }
    //Then we add the notes
    for (int i = 0; i < sizeof(R_played_note); ++i) {
        if (R_played_note[i] && (!R_played_note_prev[i] || bellow_not_null != bellow_prev)) { //We must also relaunch note if bellow changed direction
            if (bassoon_enable) {
                midi_broadcast_note_on(i+12*octave-12, expression_resolved, channel_rh);
            }
            if (picolo_enable) {
                midi_broadcast_note_on(i+12*octave+12, expression_resolved, channel_rh);
            }
            if (flute_enable) {
                midi_broadcast_note_on(i+12*octave, expression_resolved, channel_rh);
            }
        }
    }
    //Again, we do vibrato separatedly even if it duplicates a lot of code
    for (int i = 0; i < sizeof(R_played_note); ++i) {
        if (R_played_note[i] && (!R_played_note_prev[i] || bellow_not_null != bellow_prev)) { //We must also relaunch note if bellow changed direction
            if (vibrato!=0) {
                midi_broadcast_note_on(i+12*octave, expression_resolved, VIBRATO_CHANNEL);
            }
        }
    }


    //Left hand on channel_lh
    //Remove all the notes first
    for (int i = 0; i < sizeof(L_played_note); ++i) {
        if (!L_played_note[i] && L_played_note_prev[i]) {
            midi_broadcast_note_off(i, 0, channel_lh);
        }
    }
    for (int i = 0; i < sizeof(L_played_note); ++i) {
        if (L_played_note[i] && (!L_played_note_prev[i] || bellow_not_null != bellow_prev)) {
            midi_broadcast_note_on(i, expression_resolved, channel_lh);
        }
    }


    //Send Volume
    if (bellow != NOPUSH) {bellow_prev = bellow;}
    if(volume_prev!=volume_resolved){
        midi_broadcast_control_change(MIDI_CC_VOLUME, volume_resolved, channel_lh);
        midi_broadcast_control_change(MIDI_CC_VOLUME, volume_resolved, channel_rh);
        if(vibrato!=0){
            midi_broadcast_control_change(MIDI_CC_VOLUME, volume_resolved, VIBRATO_CHANNEL);
        }
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
            str_oled[i] = '\0';
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
