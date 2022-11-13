/* Minimal set of function to interract with the MT32 synthetiser */


#include <MIDI.h>

#define MT32_MT32 0x0
#define MT32_SOUNDFONT 0x1


void mt32_send_sysex(uint8_t cmd, uint8_t arg, midi::MidiInterface<HardwareSerial, midi_setting_running_status_low_sysex> MIDI) {
    uint8_t data[5];
    data[0]=0xF0; //MT32
    data[1]=0x7d; //MT32
    data[2]=cmd;
    data[3]=arg;
    data[4]=0xf7;
    MIDI.sendSysEx(5, data, 1);
}
void mt32_send_short_sysex(uint8_t cmd, midi::MidiInterface<HardwareSerial, midi_setting_running_status_low_sysex> MIDI) {
    uint8_t data[4];
    data[0]=0xF0; //MT32
    data[1]=0x7d; //MT32
    data[2]=cmd;
    data[3]=0xf7;
    MIDI.sendSysEx(4, data, 1);
}
void mt32_switch_rom_set(uint8_t set, midi::MidiInterface<HardwareSerial, midi_setting_running_status_low_sysex> MIDI) {
    mt32_send_sysex(0x01, set, MIDI);
}


void mt32_switch_soundfont(uint8_t idx, midi::MidiInterface<HardwareSerial, midi_setting_running_status_low_sysex> MIDI) {
    mt32_send_sysex(0x02, idx, MIDI);
}

void mt32_switch_synth(uint8_t synth, midi::MidiInterface<HardwareSerial, midi_setting_running_status_low_sysex> MIDI) {
    mt32_send_sysex(0x03, synth, MIDI);
}
void mt32_reboot(midi::MidiInterface<HardwareSerial, midi_setting_running_status_low_sysex> MIDI) {
    mt32_send_short_sysex(0x00,MIDI);
}