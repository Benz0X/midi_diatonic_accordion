#include <MIDI.h>

/* Correspondance between midi notes and value */

#define mid_C0            0
#define mid_D0            2
#define mid_E0            4
#define mid_F0            5
#define mid_G0            7
#define mid_A0            9
#define mid_B0           11
#define mid_C1           12
#define mid_D1           14
#define mid_E1           16
#define mid_F1           17
#define mid_G1           19
#define mid_A1           21
#define mid_B1           22
#define mid_C2           24
#define mid_D2           26
#define mid_E2           28
#define mid_F2           29
#define mid_G2           31
#define mid_A2           33
#define mid_B2           35
#define mid_C3           36
#define mid_D3           38
#define mid_E3           40
#define mid_F3           41
#define mid_G3           43
#define mid_A3           45
#define mid_B3           47
#define mid_C4           48
#define mid_D4           50
#define mid_E4           52
#define mid_F4           53
#define mid_G4           55
#define mid_A4           57
#define mid_B4           59
#define mid_C5           60
#define mid_D5           62
#define mid_E5           64
#define mid_F5           65
#define mid_G5           67
#define mid_A5           69
#define mid_B5           71
#define mid_C6           72
#define mid_D6           74
#define mid_E6           76
#define mid_F6           77
#define mid_G6           79
#define mid_A6           81
#define mid_B6           83
#define mid_C7           84
#define mid_D7           86
#define mid_E7           88
#define mid_F7           89
#define mid_G7           91
#define mid_A7           93
#define mid_B7           95
#define mid_C8           96
#define mid_D8           98
#define mid_E8           100
#define mid_F8           101
#define mid_G8           103
#define mid_A8           105
#define mid_B8           107
#define mid_C9           108
#define mid_D9           110
#define mid_E9           112
#define mid_F9           113
#define mid_G9           115
#define mid_A9           117
#define mid_B9           119
#define SHARP         1
#define FLAT         -1
#define OCTAVE       12


/* Minimal set of functions for MIDI */
void midi_send_master_volume(uint8_t volume, midi::MidiInterface<HardwareSerial> MIDI) {
    uint8_t data[6];
    data[0]=0x7f; //REALTIME
    data[1]=0x7f; //Disregard
    data[2]=0x04; //SubID Device control
    data[3]=0x01; //SubID Master volume
    data[4]=volume; //LSB
    data[5]=volume; //MSB
    MIDI.sendSysEx(8, data, 0);
}
