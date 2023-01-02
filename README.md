# midi_diatonic_accordion
This project is my attempt at creating a MIDI accordion using an arduino board with an optional Raspberry Pi 3 as a sound synthetiser using the awesome [Mt32-Pi project](https://github.com/dwhinham/mt32-pi)

Because of hearing problems, I am limited in my use of acoustics instruments, and having an instrument with the ability to play at any volume I want allows me to practice a little.

The [Roland FR-18](https://www.roland.com/global/products/fr-18_diatonic/) would be the commercial equivalent but it is expensive, no longer in production and making your own instrument is a lot of fun !

This project and document are not intended as tutorial, guides or even directly usable. However, I hope it can give ideas or be of use to anyone who wants to build its own MIDI instrument.

## Goals
- Having an instrument capable of generating MIDI signals and sending it via USB or UART
- Pressure sensitive (for both bellow Push/Pull detection and volume control). I play a diatonic accordion but building a chromatic one would be similar.
- Having a 'real' instrument feel (this is why I used an old accordion)
- Onboard sound synthesis with a 3.5mm stereo jack output (optionnal but nice to have, uses a Raspberry Pi 3 or higher)
    - Without onboard sound synthesis, a synthetiser or a computer is needed. On windows, this can be achieved with [Hairless-MIDISERIAL](https://projectgus.github.io/hairless-midiserial/), [LoopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) and any synth software (FluidSynth, Kontakt...)
- Powered by a single USB cable
- LCD/OLED screen with menu based interface for settings
- Reasonable budget (including instrument, controller and electronics)
- As responsive as possible
- Sounds like an actual accordion
- Be able to transpose or change keyboard layout easily

## Hardware
### Accordion
For the hardware side, I wanted to have a real instrument, I bought a cheap, probably around 60 years old accordion that fitted my needs:
- Bellow without too much leaks
- Three rows of buttons on the right hand side (34 total)
- Four rows of buttons on the left hand side (I needed a specific [4*6 layout](https://www.diatoz.fr/app/download/13875905733/plan+DARWIN+2+vierge+34+24.pdf?t=1532170869) as on my 'real' accordion)

This is an Organola Amati 3

I did not care if the instrument was made unusable in the process, so I removed all the reeds blocs and some pieces of (now) useless wood. I removed 24 of the 48 left hand buttons and modified the left hand mechanism so that one button pressed would only raise one valve flap so I had a bijection between the 24 valve flaps and the 24 buttons.

It would be possible to do it without modifying the instruments too much, but it would make the integration a bit more complex. (some company can mount MIDI kits in an existing instrument, there are also some [DIY](https://www.youtube.com/watch?v=oKxq-bHDVL4&t=44s))

I also changed the bellow seal (for better compression) and filled some holes with a glue gun.

<figure>
  <img src="https://github.com/Benz0X/midi_diatonic_accordion/blob/main/doc/image/outside_front.jpeg?raw=true" alt="Outside, front" width="400"/>
  <figcaption>The accordion front, with ON/OFF switchs and jack connector visibles</figcaption>
</figure>

### Electronic
The main challenge was to build reliable switchs to detect if a key is pressed or not. One possibility is to build your own keyboard using mechanical keyboard switches but I wanted to have a real keyboard feel. I decided to use Hall effect sensors but it can be done with opto switches (you can check [this](https://bvavra.github.io/MIDI_Accordion/overview/) well documented project).

I needed one Hall sensor (I built my first prototype with A3214 sensors and switched to cheaper AH3574-P which consume a little more power, double check your power budget !) and one magnet per switch (I chose 4mmx2mm round magnets on ebay).

I glued one magnet to the middle of each valve flap, put tape over the holes from the inside of the accordion to have less leaks, and put a sensor in front of each magnet (assembled on small prototyping board first, then on a homemade PCB). A bit of tuning is necessary to put the proper distance between the magnet and the sensor but it works quite well. Be advised that a LOT of soldering is required !

Having 34 buttons on the right hand and 24 on the left hand, I didn't have enough pins on the Arduino to connect everything (and I didn't want 30 cables between left and right hand) so I used two I2C GPIO expander MCP23017 for each hands (the two missing buttons on the right hand are connected as GPIOs directly to the Arduino)

Adding capacitor next to the Hall sensor is recommanded in the specification, but my design worked correctly with only a few capacitors.

Another option to reduce the number of IOs needed would be to do a matrix scan of the switchs, but it requires diods and as the Hall Effect switch have a non null wake up time, this could lower the refresh rate and reduce the reactivity of the instrument.

For the pressure sensor, I used a BMP180 (since replaced by the BMP280).

After correctly wiring the addresses for the MCP23017, it is only needed to connect everything together, which is quite tedious but not complicated.

<figure>
  <img src="https://github.com/Benz0X/midi_diatonic_accordion/blob/main/doc/image/inside_rh_cable.jpeg?raw=true" alt="Inside rh cables" width="400"/>
  <figcaption>The accordion right hand, viewed from the inside before use of the PCB</figcaption>
</figure>
<figure>
  <img src="https://github.com/Benz0X/midi_diatonic_accordion/blob/main/doc/image/inside_lh_cable.jpeg?raw=true" alt="Inside lh cables" width="400"/>
  <figcaption>The accordion left hand, viewed from the inside before use of the PCB</figcaption>
</figure>
<figure>
  <img src="https://github.com/Benz0X/midi_diatonic_accordion/blob/main/doc/image/inside_rh_pcb.jpeg?raw=true" alt="Inside rh pcb" width="200"/>
  <figcaption>The accordion right hand, with PCB (version without the Arduino)</figcaption>
</figure>
<figure>
  <img src="https://github.com/Benz0X/midi_diatonic_accordion/blob/main/doc/image/inside_lh_pcb.jpeg?raw=true" alt="Inside lh pcb" width="200"/>
  <figcaption>The accordion left hand, with PCB</figcaption>
</figure>


### Boards and USB
I used an Arduino Due as it provides a lot of space and adequate speed (and I had one available). The system might fit on smaller Arduinos, the program currently use around 58kB of FLASH and 6kB of RAM.

I used a Raspberry Pi3B+ with an I2S DAC for the audio synthesis (required only if onboard sound synthesis is needed). The I2S DAC is mandatory for good sound, and cooling is also required as the PI can output a lot of heat (I put a small fan on top of a heatsink).

Both board are connected to an ON/OFF switch mounted on the front of the accordion to be able to individually power ON/OFF each board. I used one usb cable with the power and ground going to the switchs, and the data going to the Arduino only (the Raspberry Pi does not have data signals on the USB).

To avoid strange audio glitch, I have to power the PI first and then reboot it with a MT32 command (this is explained in the code).

To avoid voltage drop, the USB cable must not be too long and of good quality.

The whole system can be powered by a PC USB port or 1A plug.

### PCB
In order to improve the first prototype and its unreliable wiring, I decided to make a PCB. I used EasyEDA and ordered at JLCPCB as it was the most easy to use and cheapest option.

The minimum quantity to order being 5 units, I decided to make a PCB compatible with both right and left hand of the accordion.

The PCB schematics are available in the doc folder.

### BOM
Here are the approximate prices for components. Most electronics has been bought at Mouser, magnets, prototyping board and wires on eBay.

|Item|Price(€)|
|----|---|
|Accordion|150   |
|Arduino Due|42   |
|MCP23017 x4 |10   |
|DIP28 socket x4 (to prevent having to solder the component directly)    |4  |
|AH3574-P-B  x100 (cheaper to buy 100 than 60)    |28  |
|100pF capacitors  x100    |10  |
|2x4mm round magnet x100    |11  |
|OLED screen (SSD1306 based)    |7  |
|BMP180 board    |2  |
|Various cables (dupont, ribbon, headers...)    |20  |
|PCB (optionnal) |25  |
|prototyping board (optionnal)    |10  |
|RaspberryPi 3b (optionnal)    |out of stock for now  |
|PCM5102 I2S DAC (optionnal)    |13  |
|On/Off switch  (optionnal)    |2  |
|Female jack plug  (optionnal)    |2  |


Without the Raspberry Pi, the price of the total is around 300€, half of which being the instrument.


## Software
I am not a C/C++ dev so my code will probably looks terrible to someone coming from the software side, but it works !
### Arduino
The software in the arduino relies heavily on external libraries for the various aspects (MCP23017, screen, menus, MIDI, pressure...).

The code will not be thoroughly documented in this readme and is hopefully commented enough to be understandable, but it can be summarized in a few short steps:
- Pressure gathering (if available, the BMP180 refresh rate is quite low). Pressure is converted to volume using various mathematical functions (I am not fully convinced by those functions, other curves should be explored)
- Key gathering from MCP23017 and GPIOs
- If menu key is pressed (one of the 34 Right Hand key is dedicated to control the menu), update menu and display. This also handles preset and stops (to quickly toggle virbrato or bassoon for instance)
- Convert pressed/depressed keys and pressure into MIDI messages
- Send the MIDI messages to UART (to the Raspberry Pi) and to USB (optionnal)

Through the menu, you can configure a lot of parameters for MIDI (program, channel, reverb and chorus...) and for MT32Pi (when connected to a Raspberry Pi: soundfont selection, synth selection...)

### Raspberry Pi
The Raspberry Pi runs an instance of the MT32-PI software which run Fluidsynth and Munt to synthetise either SoundFonts or the Roland MT32 synthetiser.

I found an amazing accordion soundfont of a Bernard Loffet accordion [here](http://jmi.ovh/DiatonicTab/soundfonts.html) which does really sound like an accordion, but it can of course play General Midi soundfont or any other.

The setup is quite easy and well explained on the MT32-PI documentation (best results were achieved with I2S, 32kHz sampling rate and 32 sample buffer size).

The RPI RX port must be connected to the Arduino TX port.



A follow up of this project to run only on the RPI (removing the need of the arduino) is available [here](https://github.com/Benz0X/mt32-pi_x_midi_diato)

## Acknowledgments
I would like to express my thanks to a few people:
- [Dale Whinham](https://github.com/dwhinham) for his amazing MT32PI project
- [Pierre Banwarth](https://github.com/PierreBanwarth) for the inspiration and the technical discussions
- The other MIDI accordions projects that I thouroughly checked ([Brendan Vavra](https://bvavra.github.io/MIDI_Accordion/), [Dmitry Yegorenkov](https://github.com/accordion-mega/AccordionMega/wiki/Accordion-Mega-story), [Lee O'Donnell](http://www.bassmaker.co.uk/))
- The arduino library creators that makes it so easy to use external devices
- [JMiB](http://jmi.ovh/DiatonicTab/soundfonts.html) for the amazing accordion soundfont
