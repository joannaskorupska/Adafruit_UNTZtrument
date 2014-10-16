// Simple UNTZtrument MIDI step sequencer.
// Requires an Arduino Leonardo w/TeeOnArdu config (or a PJRC Teensy),
// software on host computer for synth or to route to other devices.


// todos
// add C's as a horizontal line when scrolling up and down the scale
// allow transpositions between scales
// add measure marks as vertical lines when scrolling left and right
// allow selection of measure size and number of measures by making a box
// allow making notes stay on when multiples are in a row instead of re keying them
// add saving to eeprom of current set
// add performance layer to be able to play along, each key is a different note so you have access to the entire keyboard and can map keys to multiple instruments
// accept midi input to set the lights on the performance layer or just set and clear the lights ourself if no one is send us anything
// add other modes for games like lights out and tetris and break out and match two cards
// add built in simple synth for audio output, best we can do with space available, add speaker and holes and headphone jack
// add deluxe synth board and hook it up with internal chargable battery and speaker and headphone jack
// add flicker to UI display indicators to vertical and horizontal bars so they are distict from the notes
// add flicker from notes on other layers when playing notes, maybe not as much as the tanori
// add computer generated music or accompiment
// support less than 8 wide grid when scrolling and disable keys on right side
// make one layer a performance layer to save space in EEPROM for extra info like channelss and stuff
// always store into eeprom as things are changed so we don't need an explicit save function, need to check lifetime writes
// reduce width to 16 notes max from 32 so all 8 layers and musics can be stored in eeprom minus the perfomrnace layer
// add a playlist to play complete song based on layer sets, requires measures to have the same width
// need to properly restore volumes, instruments, tempo, etc. when switching sets and layers
// need to send out current volumes and instrumnets at startup, maybe at the start of each measure
// still have trouble with notes all off when switch layers and grid sets
// need to check if long notes still work when changing button grid marks or layer and other switches
// need a scale with just the mt32 drumkit notes
// volumes left are right controll (fade)
// flash notes on other layers as they are played so we can see where the notes are being played on other layers
// add load/save state by midi note export and recording on an external device
// need to switch to new instrument on grid switch
// maybe hide play bar when scrolling to avoid confusion

#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_Trellis.h>
#include <Adafruit_UNTZtrument.h>

#ifndef HELLA
// A standard UNTZtrument has four Trellises in a 2x2 arrangement
// (8x8 buttons total).  addr[] is the I2C address of the upper left,
// upper right, lower left and lower right matrices, respectively,
// assuming an upright orientation, i.e. labels on board are in the
// normal reading direction.
Adafruit_Trellis     T[4];
Adafruit_UNTZtrument untztrument(&T[0], &T[1], &T[2], &T[3]);
const uint8_t        addr[] = {  
  0x70, 0x71, 0x72, 0x73 };
#else
// A HELLA UNTZtrument has eight Trellis boards...
Adafruit_Trellis     T[8];
Adafruit_UNTZtrument untztrument(&T[0], &T[1], &T[2], &T[3], &T[4], &T[5], &T[6], &T[7]);
const uint8_t        addr[] = { 
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77 };
#endif // HELLA

#define BUTTON_GRID_WIDTH     ((sizeof(T) / sizeof(T[0])) * 2)
#define N_BUTTONS ((sizeof(T) / sizeof(T[0])) * 16)

// Encoder on pins 4,5 sets tempo.  Optional, not present in
// standard UNTZtrument, but won't affect things if unconnected.
enc eScaleOffset(5, 4);
enc eDisplayOffset(9, 8);
enc eTempo(7, 6);
enc eVolume(11, 10);

#define BUTTON_WIDTH            13
#define BUTTON_GRID             0
#define BUTTON_PROGRAM_CHANGE   1
#define BUTTON_LAYER            12

uint8_t       col          = 0;            // Current column
unsigned int  bpm          = 240;          // Tempo
unsigned long beatInterval = 60000L / bpm; // ms/beat
unsigned long prevBeatTime = 0L;           // Column step timer
unsigned long prevReadTime = 0L;           // Keypad polling timer
uint8_t       playing_grid_set = 0;
uint8_t       visible_grid_set = 0;
uint8_t       layer = 0;                   // current layer
//todo need a next layer so layer switches do not leave notes on
uint8_t       display_offset = 0;          // current horizontal display offset
boolean       force_first_note = false;

// defines size of sequencer
#define LAYERS         8
#define GRID_WIDTH     16
#define GRID_SETS      8

uint8_t       grid[GRID_WIDTH][LAYERS][GRID_SETS]; // bitfield

uint8_t       channels[LAYERS][GRID_SETS];  // 1 to 127, 1 bit free
boolean       long_notes[LAYERS][GRID_SETS] = {
  1, 1, 1, 1, 1, 1, 1, 0}; // 0 to 1, 7 bits free
uint8_t       noteOffset[LAYERS][GRID_SETS]; // 0 to (MAX_MIDI_NOTES-BUTTON_GRID_WIDTH), 2 bits free
uint8_t       volume[LAYERS][GRID_SETS]; // 0 to 127, 1 bit free
uint8_t       instrument[LAYERS][GRID_SETS]; // 1 to 127, 1 bit free
uint8_t       scale_notes[LAYERS][GRID_SETS]; // 0 to (MAX_SCALES-1), 4 bits free
// total of 16 bits not used, 128 bytes packed

uint8_t       grid_width[GRID_SETS];  // 0 to (GRID_WIDTH-1) 4 bits free
uint8_t       tempo[GRID_SETS]; // todo hook this up to bpm
// total of 384 + 1024 + 16
// todo need to get this under 1K to be able ot store everything in EEPROM

//uint8_t max_grid_width_table[14] = {
//  8, 9, 10, 12, 14, 15, 16, 20, 21, 24, 25, 27, 28, 30}; // mutiples of 2, 3, 4, 5, 6, 7 that are equal to or more than 8

#define MIDI_MIDDLE_C 60
// todo make this 64 so I can use all the buttons on the performance layer (maybe 56 so that we have 8 buttons to switch grid sets on and have an even 7 buttons across for scales)
#define MAX_MIDI_NOTES 54

#define MAX_SCALES 12

// these are stored in program memory so they do not take up ram and can be stored unpacked for quick easy access
// todo add many more scales, since we have pleanty of program space
// add MT32 drum channel scale with only active notes, default to last layer which defaults to channel 10
static const uint8_t PROGMEM
scales[MAX_SCALES][MAX_MIDI_NOTES] = {
  {
    105, 103, 101, 100, 98, 96, 95, 93, 91, 89, 88, 86, 84, 83, 81, 79, 77, 76, 74, 72, 71, 69, 67, 65, 64, 62, 60, 59, 57, 55, 53, 52, 50, 48, 47, 45, 43, 41, 40, 38, 36, 35, 33, 31, 29, 28, 26, 24, 23, 21, 19, 17, 16, 14          }
  ,
  {
    104, 103, 101, 99, 98, 96, 94, 92, 91, 89, 87, 86, 84, 82, 80, 79, 77, 75, 74, 72, 70, 68, 67, 65, 63, 62, 60, 58, 56, 55, 53, 51, 50, 48, 46, 44, 43, 41, 39, 38, 36, 34, 32, 31, 29, 27, 26, 24, 22, 20, 19, 17, 15, 14          }
  ,
  {
    86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33          }
  ,
  {
    113, 111, 108, 106, 103, 102, 101, 99, 96, 94, 91, 90, 89, 87, 84, 82, 79, 78, 77, 75, 72, 70, 67, 66, 65, 63, 60, 58, 55, 54, 53, 51, 48, 46, 43, 42, 41, 39, 36, 34, 31, 30, 29, 27, 24, 22, 19, 18, 17, 15, 12, 10, 7, 6          }
  ,
  {
    99, 97, 96, 94, 93, 91, 90, 88, 87, 85, 84, 82, 81, 79, 78, 76, 75, 73, 72, 70, 69, 67, 66, 64, 63, 61, 60, 58, 57, 55, 54, 52, 51, 49, 48, 46, 45, 43, 42, 40, 39, 37, 36, 34, 33, 31, 30, 28, 27, 25, 24, 22, 21, 19          }
  ,
  {
    95, 94, 93, 92, 91, 89, 87, 86, 84, 83, 82, 81, 80, 79, 77, 75, 74, 72, 71, 70, 69, 68, 67, 65, 63, 62, 60, 59, 58, 57, 56, 55, 53, 51, 50, 48, 47, 46, 45, 44, 43, 41, 39, 38, 36, 35, 34, 33, 32, 31, 29, 27, 26, 24          }
  ,
  {
    104, 103, 101, 100, 98, 96, 95, 92, 91, 89, 88, 86, 84, 83, 80, 79, 77, 76, 74, 72, 71, 68, 67, 65, 64, 62, 60, 59, 56, 55, 53, 52, 50, 48, 47, 44, 43, 41, 40, 38, 36, 35, 32, 31, 29, 28, 26, 24, 23, 20, 19, 17, 16, 14          }
  ,
  {
    104, 103, 101, 99, 98, 96, 95, 92, 91, 89, 87, 86, 84, 83, 80, 79, 77, 75, 74, 72, 71, 68, 67, 65, 63, 62, 60, 59, 56, 55, 53, 51, 50, 48, 47, 44, 43, 41, 39, 38, 36, 35, 32, 31, 29, 27, 26, 24, 23, 20, 19, 17, 15, 14          }
  ,
  {
    105, 103, 101, 99, 98, 96, 95, 93, 91, 89, 87, 86, 84, 83, 81, 79, 77, 75, 74, 72, 71, 69, 67, 65, 63, 62, 60, 59, 57, 55, 53, 51, 50, 48, 47, 45, 43, 41, 39, 38, 36, 35, 33, 31, 29, 27, 26, 24, 23, 21, 19, 17, 15, 14          }
  ,
  {
    111, 110, 108, 107, 105, 103, 99, 98, 96, 95, 93, 91, 87, 86, 84, 83, 81, 79, 75, 74, 72, 71, 69, 67, 63, 62, 60, 59, 57, 55, 51, 50, 48, 47, 45, 43, 39, 38, 36, 35, 33, 31, 27, 26, 24, 23, 21, 19, 15, 14, 12, 11, 9, 7          }
  ,
  {
    105, 102, 101, 100, 97, 96, 94, 93, 90, 89, 88, 85, 84, 82, 81, 78, 77, 76, 73, 72, 70, 69, 66, 65, 64, 61, 60, 58, 57, 54, 53, 52, 49, 48, 46, 45, 42, 41, 40, 37, 36, 34, 33, 30, 29, 28, 25, 24, 22, 21, 18, 17, 16, 13          }
  ,
  {
    103, 102, 101, 100, 98, 96, 94, 91, 90, 89, 88, 86, 84, 82, 79, 78, 77, 76, 74, 72, 70, 67, 66, 65, 64, 62, 60, 58, 55, 54, 53, 52, 50, 48, 46, 43, 42, 41, 40, 38, 36, 34, 31, 30, 29, 28, 26, 24, 22, 19, 18, 17, 16, 14          }
};

/*
// todo need to store these at 16 bit numbers with byte length embedded in the top 4 bits, or store unpacked in program memory
 static const uint8_t majorScale[7] = {
 0,2,4,5,7,9,11};
 static const uint8_t minorScale[7] = {
 0,2,3,5,7,8,10};
 static const uint8_t chromaticScale[12] = {
 0,1,2,3,4,5,6,7,8,9,10,11};
 static const uint8_t bluesScale[] = {
 0,3,5,6,7,10};
 static const uint8_t bluesdiminishedScale[]={
 0,1,3,4,6,7,9,10};
 static const uint8_t fullMinorScale[]={
 0,2,3,5,7,8,9,10,11};
 static const uint8_t harmonicMajorScale[]={
 0,2,4,5,7,8,11};
 static const uint8_t harmonicMinorScale[]={
 0,2,3,5,7,8,11};
 static const uint8_t jazzminorScale[]={
 0,2,3,5,7,9,11};
 static const uint8_t hawaiianScale[]={ 
 0,2,3,7,9,11};
 static const uint8_t orientalScale[]={
 0,1,4,5,6,9,10};
 static const uint8_t majorMinotScale[]={
 0,2,4,5,6,7,10};
 
 uint8_t notes[MAX_MIDI_NOTES];
 
 void makenotes(uint8_t * scale, uint8_t length)
 {
 for (int index = 0; index < MAX_MIDI_NOTES/2; index++)
 {
 uint8_t scale_offset = ((index / length) * 12);
 uint8_t index_wrap = (index % length);
 notes[MAX_MIDI_NOTES/2 + index] = MIDI_MIDDLE_C - (12 - scale[length - 1 - index_wrap]) - scale_offset;
 notes[MAX_MIDI_NOTES/2 - index - 1] = MIDI_MIDDLE_C + scale[index_wrap] + scale_offset;
 }
 }
 */

void saveToEEPROM(int eepromAddress, uint8_t * source, int length)
{
  for (int index = 0; index < length; index++)
  {
    EEPROM.write(eepromAddress + index, source[index]);  
  }
}

void loadFromEEPROM(int eepromAddress, uint8_t * source, int length)
{
  for (int index = 0; index < length; index++)
  {
    source[index] = EEPROM.read(eepromAddress + index);  
  }
}

//todo add checksums to when things change we reset them
void save(void)
{
  int address = 0;
  saveToEEPROM(address, (uint8_t *)&grid[0], sizeof(grid));
  //  address += sizeof(grid);
  //  saveToEEPROM(address, (uint8_t *)&grid_width[0], sizeof(grid_width));
  //  address += sizeof(grid_width);
  //  saveToEEPROM(address, (uint8_t *)&noteOffset[0], sizeof(noteOffset));
  //  address += sizeof(noteOffset);
}

void load(void)
{
  int address = 0;
  loadFromEEPROM(address, (uint8_t *)&grid[0], sizeof(grid));
  //  address += sizeof(grid);
  //  loadFromEEPROM(address, (uint8_t *)&grid_width[0], sizeof(grid_width));
  //  address += sizeof(grid_width);
  //  loadFromEEPROM(address, (uint8_t *)&noteOffset[0], sizeof(noteOffset));
  //  address += sizeof(noteOffset);
}

void setup() 
{
  pinMode(BUTTON_WIDTH, INPUT_PULLUP); 
  pinMode(BUTTON_GRID, INPUT_PULLUP);
  pinMode(BUTTON_PROGRAM_CHANGE, INPUT_PULLUP);
  pinMode(BUTTON_LAYER, INPUT_PULLUP);

#ifndef HELLA
  untztrument.begin(addr[0], addr[1], addr[2], addr[3]);
#else
  untztrument.begin(addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
#endif // HELLA
#ifdef __AVR__
  // Default Arduino I2C speed is 100 KHz, but the HT16K33 supports
  // 400 KHz.  We can force this for faster read & refresh, but may
  // break compatibility with other I2C devices...so be prepared to
  // comment this out, or save & restore value as needed.
  TWBR = 12;
#endif

  untztrument.clear();
  untztrument.writeDisplay();

  memset(grid, 0, sizeof(grid)); // no notes set yet, all grid sets and layers
  memset(noteOffset, 23, sizeof(noteOffset)); // current center of note scale for each layer and grid set
  memset(volume, 64, sizeof(volume)); // 1/2 range current midi volume for each layer
  memset(instrument, 0, sizeof(instrument)); // current midi instrument for each layer
  memset(grid_width, 16, sizeof(grid_width)); // default all grids to 16 beats wide
  memset(scale_notes, 0, sizeof(scale_notes)); // default all grids to major scale
  memset(channels, 2, sizeof(channels)); // default all channels to 2

  for (uint8_t index = 0; index < GRID_SETS; index++) // default all the last layers to channel 10 for mt32 drum channel
  {
    channels[0][index] = 3;
    channels[1][index] = 4;
    channels[2][index] = 5;
    channels[3][index] = 6;
    channels[4][index] = 7;
    channels[5][index] = 8;
    channels[6][index] = 10;
    channels[7][index] = 2;
  }

  //  load();

  eScaleOffset.setBounds(0, 44 * 4 + 3); // Set note offset limits
  eScaleOffset.setValue(noteOffset[layer][visible_grid_set] * 4);              // *4's for encoder detents
  eTempo.setBounds(20 * 4, 1000 * 4 + 3); // Set tempo limits
  eTempo.setValue(bpm * 4);              // *4's for encoder detents
  eDisplayOffset.setBounds(0, (grid_width[visible_grid_set] - BUTTON_GRID_WIDTH) * 4 + 3); // Set diplay offset limits
  eDisplayOffset.setValue(0);              // *4's for encoder detents
  eVolume.setBounds(0, 127 * 4 + 3); // Set volume limits
  eVolume.setValue(volume[layer][visible_grid_set] * 4);              // *4's for encoder detents
}

bool flicker_state = true;

// Turn on (or off) one column of the display
void line_vert(uint8_t x, boolean setit) 
{
  for(uint8_t mask=1, y=0; y<8; y++, mask <<= 1) 
  {
    uint8_t i = untztrument.xy2i(x, y);
    if((setit && flicker_state) || (grid[x + display_offset][layer][visible_grid_set] & mask)) 
    {
      untztrument.setLED(i);
    }
    else          
    {
      untztrument.clrLED(i);
    }
  }
}

// Turn on (or off) one row of the display
void line_horz(uint8_t y, boolean setit) 
{
  uint8_t mask = 1 << y;
  for(uint8_t column_index=0; column_index < 8; column_index++) 
  {
    uint8_t i = untztrument.xy2i(column_index, y);
    if((setit && flicker_state) || (grid[column_index + display_offset][layer][visible_grid_set] & mask)) 
    {
      untztrument.setLED(i);
    }
    else          
    {
      untztrument.clrLED(i);
    }
  }
}

// sequencer UI state modes
#define PLAYING                         1
#define WAIT_FOR_LAYER                  2
#define PLAYING_ERASE_LAYER_1           3
#define PLAYING_ERASE_LAYER_2           4
#define WAIT_FOR_PROGRAM_CHANGE         5
#define WAIT_FOR_PROGRAM_CHANGE_2       6
#define PLAYING_ERASE_PROGRAM_CHANGE_1  7
#define PLAYING_ERASE_PROGRAM_CHANGE_2  8
#define WAIT_FOR_GRID                   9
#define PLAYING_ERASE_GRID_1            10 
#define PLAYING_ERASE_GRID_2            11
#define WAIT_FOR_WIDTH                  12
#define PLAYING_ERASE_WIDTH_1           13 
#define PLAYING_ERASE_WIDTH_2           14
#define WAIT_FOR_SCALE                  15
#define PLAYING_ERASE_SCALE_1           16
#define PLAYING_ERASE_SCALE_2           17
#define PLAYING_ERASE_C_1               18 
#define PLAYING_ERASE_C_2               19
#define PLAYING_ERASE_MEASURE_1         20 
#define PLAYING_ERASE_MEASURE_2         21
#define WAIT_FOR_CHANNEL                22
#define PLAYING_ERASE_CHANNEL_1         23
#define PLAYING_ERASE_CHANNEL_2         24

// default sequencer state is playing
uint8_t mode = PLAYING;

// button states, with last val for debounce 
uint8_t sensor0Val = HIGH;
uint8_t lastSensor0Val = HIGH;
uint8_t sensor1Val = HIGH;
uint8_t lastSensor1Val = HIGH;
uint8_t sensor12Val = HIGH;
uint8_t lastSensor12Val = HIGH;
uint8_t sensor13Val = HIGH;
uint8_t lastSensor13Val = HIGH;

void loop() 
{
  uint8_t       mask;
  boolean       refresh = false;
  unsigned long t       = millis();

  enc::poll(); // Read encoder(s)

  //flicker_state = !flicker_state;
  //refresh = true;

  if((t - prevReadTime) >= 20L) // 20ms = min Trellis poll time
  {
    
    if(untztrument.readSwitches()) // Button state change?
    { 
      for(uint8_t i=0; i<N_BUTTONS; i++) // For each button...
      { 
        uint8_t x, y;
        untztrument.i2xy(i, &x, &y);
        mask = 1 << y;
        if(untztrument.justReleased(i) && (layer == LAYERS-1)) 
        {
          if (x != 7)
          {
            uint8_t temp = channels[layer][playing_grid_set];
            usbMIDI.sendNoteOff(pgm_read_byte(&scales[scale_notes[layer][playing_grid_set]][8-x + y*7] ) , 127, temp);
            untztrument.clrLED(i);

            refresh = true;
          }
        }

        if(untztrument.justPressed(i)) 
        { 
          switch (mode)
          {

    case PLAYING_ERASE_PROGRAM_CHANGE_1:
//          case PLAYING_ERASE_PROGRAM_CHANGE_2:
          case PLAYING_ERASE_CHANNEL_1:
//          case PLAYING_ERASE_CHANNEL_2:
          case PLAYING_ERASE_LAYER_1:
//          case PLAYING_ERASE_LAYER_2:
          case PLAYING_ERASE_GRID_1:
//          case PLAYING_ERASE_GRID_2:
          case PLAYING_ERASE_WIDTH_1:
//          case PLAYING_ERASE_WIDTH_2:
          case PLAYING_ERASE_SCALE_1:
//          case PLAYING_ERASE_SCALE_2:
          case PLAYING_ERASE_C_1:
//          case PLAYING_ERASE_C_2:
          case PLAYING_ERASE_MEASURE_1:
//          case PLAYING_ERASE_MEASURE_2:
            // fall through
          case PLAYING:
            if (layer != LAYERS-1)
            {
              uint8_t * temp = &grid[x + display_offset][layer][visible_grid_set];
              if(*temp & mask) // Already set?  Turn off...
              { 
                *temp &= ~mask;
                untztrument.clrLED(i);

                // need to turn it off now as we will not be turning it off later at the next beat because we just cleared the bit indicating it was playing
                //todo this wont work for other grid set is still playing
                if (visible_grid_set == playing_grid_set)
                {
                  uint8_t temp2 = channels[layer][playing_grid_set];
                  usbMIDI.sendNoteOff(pgm_read_byte(&scales[scale_notes[layer][playing_grid_set]][y + noteOffset[layer][playing_grid_set]] ) , 127, temp2);
                }
              }
              else // Turn on
              { 
                *temp |= mask;
                untztrument.setLED(i);
              }
            }
            else
            {
              if (x != 7)
              {
                uint8_t temp = channels[layer][playing_grid_set];
                usbMIDI.sendNoteOn(pgm_read_byte(&scales[scale_notes[layer][playing_grid_set]][8-x + y*7] ) , 127, temp);
                untztrument.setLED(i);
              }
              else
              {
                visible_grid_set = y;
                untztrument.setLED(i);
              }
            }
            refresh = true;
            break;

          case WAIT_FOR_PROGRAM_CHANGE:
            {
              uint8_t temp = instrument[layer][visible_grid_set];

              if (temp >= 64)
              {
                temp = temp - 64;  
              }

              // Turn off column and row
              line_vert(temp % 8, false);
              line_horz(temp / 8, false);

              temp = x+y*8;
              instrument[layer][visible_grid_set] = temp;

              usbMIDI.sendProgramChange(temp, channels[layer][playing_grid_set]);
              force_first_note = true;

              // Turn on column and row for a beat to indicate selection
              line_vert(x, true);
              line_horz(y, true);

              mode = PLAYING_ERASE_PROGRAM_CHANGE_1;
              refresh = true;
              break;
            }

          case WAIT_FOR_PROGRAM_CHANGE_2:
            {
              uint8_t temp = instrument[layer][visible_grid_set];
              if (temp >= 64)
              {
                temp = temp - 64;  
              }

              // Turn off column and row
              line_vert(temp % 8, false);
              line_horz(temp / 8, false);

              temp = x + y*8 + 64;
              instrument[layer][visible_grid_set] = temp;

              usbMIDI.sendProgramChange(temp, channels[layer][playing_grid_set]);
              force_first_note = true;

              // Turn on column and row for a beat to indicate selection
              line_vert(x, true);
              line_horz(y, true);

              mode = PLAYING_ERASE_PROGRAM_CHANGE_1;
              refresh = true;
              break;
            }

          case WAIT_FOR_LAYER:
            {
              // Turn off row
              line_horz(layer, false);

              layer = y;
              mode = PLAYING_ERASE_LAYER_1;

              // redraw whole screen because layer change
              for (uint8_t x = 0; x < 8; x++)
              {
                line_vert(x, false);
              }

              // Turn on row for a beat to indicate selection
              line_horz(layer, true);

              // adjust encoder ranges to match current layer
              eScaleOffset.setValue(noteOffset[layer][visible_grid_set] * 4);              // *4's for encoder detents
              eVolume.setValue(volume[layer][visible_grid_set] * 4);              // *4's for encoder detents

              //save(); // save on layer change, todo too slow, need to do it a byte at a time or at some point that won't hurt performance

              refresh = true;
              break;
            }

          case WAIT_FOR_GRID:
            {
              // Turn off row
              line_vert(visible_grid_set, false);

              visible_grid_set = x;
              mode = PLAYING_ERASE_GRID_1;

              // redraw whole screen because grid change
              for (uint8_t x = 0; x < 8; x++)
              {
                line_vert(x, false);
              }

              // adjust encoder ranges to match next grid set on current layer
              eScaleOffset.setValue(noteOffset[layer][visible_grid_set] * 4);              // *4's for encoder detents
              eDisplayOffset.setBounds(0, (grid_width[visible_grid_set] - BUTTON_GRID_WIDTH) * 4 + 3); // Set diplay offset limits
              eDisplayOffset.setValue(0);              // *4's for encoder detents

              // Turn on col for a beat to indicate selection
              line_vert(visible_grid_set, true);
              refresh = true;
              break; 
            }

          case WAIT_FOR_WIDTH:
            {
              uint8_t temp = grid_width[visible_grid_set] - 1;
              if (temp > GRID_WIDTH)
              {
                temp = GRID_WIDTH;
              }

              // Turn off column and row
              line_vert(temp % 8, false);
              line_horz(temp / 8, false);

              temp = x + y*8 + 1;

              if (temp > GRID_WIDTH)
              {
                temp = GRID_WIDTH;
              }
              grid_width[visible_grid_set] = temp;

              // adjust encoder ranges to match
              eDisplayOffset.setBounds(0, (temp - BUTTON_GRID_WIDTH) * 4 + 3); // Set diplay offset limits
              eDisplayOffset.setValue(0);              // *4's for encoder detents

              temp = temp - 1;

              // Turn on column and row for a beat to indicate selection
              line_vert(temp % 8, true);
              line_horz(temp / 8, true);

              mode = PLAYING_ERASE_WIDTH_1;
              refresh = true;
              break;
            }

          case WAIT_FOR_SCALE:
            {
              uint8_t temp = scale_notes[layer][visible_grid_set];

              if (temp > MAX_SCALES)
              {
                temp = MAX_SCALES - 1;  
              }

              // Turn off column and row
              line_vert(temp % 8, false);
              line_horz(temp / 8, false);

              temp = x+y*8;
              if (temp > MAX_SCALES)
              {
                temp = MAX_SCALES - 1;  
              }
              scale_notes[layer][visible_grid_set] = temp;

              force_first_note = true;

              // Turn on column and row for a beat to indicate selection
              line_vert(temp % 8, true);
              line_horz(temp / 8, true);

              mode = PLAYING_ERASE_SCALE_1;
              refresh = true;
              break;
            }

          case WAIT_FOR_CHANNEL:
            {
              uint8_t temp = channels[layer][visible_grid_set];

              // Turn off column and row
              line_vert(temp % 8, false);
              line_horz(temp / 8, false);

              temp = x+y*8;
              channels[layer][visible_grid_set] = temp;

              force_first_note = true;

              // Turn on column and row for a beat to indicate selection
              line_vert(x, true);
              line_horz(y, true);

              mode = PLAYING_ERASE_CHANNEL_1;
              refresh = true;
              break;
            }

          default:
            {
              break;
            }
          }
        }
      }
    }
    prevReadTime = t;
  }

  enc::poll(); // Read encoder(s)

  // adjust the note offset here so all notes are off when we make the change
  int temp = eScaleOffset.getValue() / 4;
  int diff = noteOffset[layer][visible_grid_set] - temp;
  if (diff != 0)
  {
    noteOffset[layer][visible_grid_set] = temp;

    // shift all notes and redraw columns
    for(uint8_t column_index=0; column_index < grid_width[visible_grid_set]; column_index++) 
    {
      if (diff > 0)
      {
        // todo need to stop the notes that will be scrolled off
        grid[column_index][layer][visible_grid_set] <<= diff;
      }
      else
      {
        grid[column_index][layer][visible_grid_set] >>= abs(diff);
      }
    }

    // shift all notes and redraw columns
    for(uint8_t row_index=0; row_index < grid_width[visible_grid_set]; row_index++) 
    {
      if (((temp + row_index) % 7) == 0) //todo this needs to actually point to the C's in a scale (%7 for the moment), scales can have different sizes but Middle C is always placed in the center of the note scale. 
      {
        line_horz(row_index, true);
        //todo need to adjust encoder value back a notch when at limit so that we can detect when it moves further at the end of travel by looking at the undivided by 4 value
      }
      else
      {
        line_horz(row_index, false);
      }
    }

    mode = PLAYING_ERASE_C_1;
    refresh      = true;
  }

  // adjust the note offset here so all notes are off when we make the change
  temp = eDisplayOffset.getValue() / 4;
  diff = display_offset - temp;
  if (diff != 0)
  {
    display_offset = temp;

    // redraw all columns
    for(uint8_t column_index=0; column_index < BUTTON_GRID_WIDTH; column_index++) 
    {
      if (((temp + column_index) % 4) == 0) //todo hard coded 4 beats a measure for indicator, need to make configured by grid width setting
      {
        line_vert(column_index, true);
        //todo need to adjust encoder value back a notch when at limit so that we can detect when it moves further at the end of travel by looking at the undivided by 4 value
      }
      else
      {
        line_vert(column_index, false);
      }
    }

    mode = PLAYING_ERASE_MEASURE_1;
    refresh      = true;
  }

  if((t - prevBeatTime) >= beatInterval) // Next beat?
  { 
    // Turn off old column
    line_vert(col - display_offset, false);

    // stop playing notes on old column on all layers
    for(uint8_t layer_index=0;layer_index<LAYERS; layer_index++) 
    {
      for(uint8_t row=0, mask=1; row<8; row++, mask <<= 1) 
      {
        if((grid[col][layer_index][playing_grid_set] & mask) && (!(grid[(col + 1) % grid_width[playing_grid_set]][layer_index][playing_grid_set] & mask) || ((col == (grid_width[playing_grid_set] - 1)) && (playing_grid_set != visible_grid_set))))
        {
          uint8_t temp = channels[layer_index][playing_grid_set];
          usbMIDI.sendNoteOff(pgm_read_byte(&scales[scale_notes[layer_index][playing_grid_set]][row + noteOffset[layer_index][playing_grid_set]] ) , 127, temp);
        }
      }
    }

    // Advance column counter, wrap around and switch grid sets if needed
    if(++col >= grid_width[playing_grid_set]) 
    {
      col = 0;
      if (playing_grid_set != visible_grid_set)
      {
        playing_grid_set = visible_grid_set;
        force_first_note = true;
      }
    }

    // adjust the volume here so all notes are off when we make the change
    // todo add display of currnet volume level with a vertical bar, possibley show other layer volumes
    temp = eVolume.getValue() / 4;
    diff = volume[layer][visible_grid_set] - temp;
    if (diff != 0)
    {
      volume[layer][visible_grid_set] = temp;
      // send change volume on channels
      usbMIDI.sendControlChange(7, temp, channels[layer][playing_grid_set]);
    }

    // Turn on new column
    line_vert(col - display_offset, true);

    // play notes on new column on all layers
    for(uint8_t layer_index=0;layer_index<LAYERS; layer_index++) 
    {
      for (uint8_t row=0, mask=1; row<8; row++, mask <<= 1) 
      {
        if((grid[col][layer_index][playing_grid_set] & mask) && ((!(grid[(col + grid_width[playing_grid_set] - 1) % grid_width[playing_grid_set]][layer_index][playing_grid_set] & mask)) || (force_first_note == true)))
        {
          uint8_t temp = channels[layer_index][playing_grid_set];
          usbMIDI.sendNoteOn(pgm_read_byte(&scales[scale_notes[layer_index][playing_grid_set]][row + noteOffset[layer_index][playing_grid_set]]), 127, temp);
          force_first_note = false; // clear trigger if it was set
        }
      }
    }

    prevBeatTime = t;
    refresh      = true;

    // todo add display of currnet tempo with a horizontal bar, possibley show other layer tempos
    bpm          = eTempo.getValue() /4 ; // Div for encoder detents
    beatInterval = 60000L / bpm;



  }

  // replace lines that may have been erased by play column movement
  switch (mode)
  {
  case PLAYING_ERASE_PROGRAM_CHANGE_1:
    mode =  PLAYING_ERASE_PROGRAM_CHANGE_2;
    // drop thru
  case WAIT_FOR_PROGRAM_CHANGE:
  case WAIT_FOR_PROGRAM_CHANGE_2:
    {
      // Turn on column and row for a beat to indicate current selection
      uint8_t temp = instrument[layer][visible_grid_set];

      if (temp >= 64)
      {
        temp = temp - 64;
      }
      line_vert(temp % 8, true);
      line_horz(temp / 8, true);
      break;
    }
  case PLAYING_ERASE_SCALE_1:
    mode =  PLAYING_ERASE_SCALE_2;
    // drop thru
  case WAIT_FOR_SCALE:
    {
      // Turn on column and row for a beat to indicate current selection
      uint8_t temp = scale_notes[layer][visible_grid_set];

      if (temp > MAX_SCALES)
      {
        temp = MAX_SCALES - 1;
      }
      line_vert(temp % 8, true);
      line_horz(temp / 8, true);
      break;
    }

  case PLAYING_ERASE_CHANNEL_1:
    mode =  PLAYING_ERASE_CHANNEL_2;
    // drop thru
  case WAIT_FOR_CHANNEL:
    {
      // Turn on column and row for a beat to indicate current selection
      uint8_t temp = channels[layer][visible_grid_set];
      line_vert(temp % 8, true);
      line_horz(temp / 8, true);
      break;
    }

  case PLAYING_ERASE_LAYER_1:
    mode =  PLAYING_ERASE_LAYER_2;
    // drop through
  case WAIT_FOR_LAYER:
    // Turn on row for a beat to indicate current selection
    line_horz(layer, true);
    break;

  case PLAYING_ERASE_GRID_1:
    mode =  PLAYING_ERASE_GRID_2;
    // drop through
  case WAIT_FOR_GRID:
    // Turn on row for a beat to indicate current selection
    line_vert(visible_grid_set, true);
    break;

  case PLAYING_ERASE_C_1:
    mode =  PLAYING_ERASE_C_2;
    break;

  case PLAYING_ERASE_MEASURE_1:
    mode =  PLAYING_ERASE_MEASURE_2;
    break;

  case PLAYING_ERASE_WIDTH_1:
    mode =  PLAYING_ERASE_WIDTH_2;
    // drop through
  case WAIT_FOR_WIDTH:
    {
      // Turn on row for a beat to indicate current selection
      uint8_t temp = grid_width[visible_grid_set] - 1;
      if (temp > GRID_WIDTH)
      {
        temp = GRID_WIDTH;
      }
      line_vert(temp % 8, true);
      line_horz(temp / 8, true);
      break;
    }

  case PLAYING_ERASE_PROGRAM_CHANGE_2:
    {
      uint8_t temp = instrument[layer][visible_grid_set];

      if (temp >= 64)
      {
        temp = temp - 64;
      }
      // Turn off column and row after selection
      line_vert(temp % 8, false);
      line_horz(temp / 8, false);
      mode =  PLAYING;
      break;
    }

  case PLAYING_ERASE_SCALE_2:
    {
      uint8_t temp = scale_notes[layer][visible_grid_set];

      if (temp > MAX_SCALES)
      {
        temp = MAX_SCALES - 1;
      }
      // Turn off column and row after selection
      line_vert(temp % 8, false);
      line_horz(temp / 8, false);
      mode =  PLAYING;
      break;
    }

  case PLAYING_ERASE_CHANNEL_2:
    {
      uint8_t temp = channels[layer][visible_grid_set];

      // Turn off column and row after selection
      line_vert(temp % 8, false);
      line_horz(temp / 8, false);
      mode =  PLAYING;
      break;
    }

  case PLAYING_ERASE_LAYER_2:
    // Turn off row after selection
    line_horz(layer, false);
    mode =  PLAYING;
    break;

  case PLAYING_ERASE_GRID_2:
    // Turn off row after selection
    line_vert(visible_grid_set, false);
    mode =  PLAYING;
    break;

  case PLAYING_ERASE_WIDTH_2:
    {
      uint8_t temp = grid_width[visible_grid_set] - 1;
      if (temp > GRID_WIDTH)
      {
        temp = GRID_WIDTH;
      }
      // Turn off column and row after selection
      line_vert(temp % 8, false);
      line_horz(temp / 8, false);
      mode =  PLAYING;
      break;
    }

  case PLAYING_ERASE_C_2:
    {
      for(uint8_t row_index=0; row_index < grid_width[visible_grid_set]; row_index++) 
      {
        line_horz(row_index, false);
      }
      mode =  PLAYING;
      break;  
    }

  case PLAYING_ERASE_MEASURE_2:
    {
      for(uint8_t column_index=0; column_index < GRID_WIDTH; column_index++) 
      {
        line_vert(column_index, false);
      }
      mode =  PLAYING;
      break;  
    }

  default:
    break;
  }
  
  enc::poll(); // Read encoder(s)

  // todo temp disable until we have a button to hook it to
  // todo make first button clear current layer, second clear current grid, third clear everything
#if 0
  // clear all layers
  sensorVal = digitalRead(???);
  if ((sensorVal == LOW) && (mode == PLAYING))
  {
    untztrument.clear();
    refresh = true;
    memset(grid, 0, sizeof(grid));

    // Send midi control change all notes off
    usbMIDI.sendControlChange(0x7b, 0, channels[layer]);
  }
#endif

  // scale select
  sensor0Val = digitalRead(BUTTON_GRID);
  if (sensor0Val != lastSensor0Val)
  {
    if ((sensor0Val == LOW) && (mode == PLAYING))
    {
      mode = WAIT_FOR_SCALE;

      // Turn on row for a beat to indicate current selection
      uint8_t temp = scale_notes[layer][visible_grid_set];

      if (temp > MAX_SCALES)
      {
        temp = MAX_SCALES - 1;    
      }

      line_vert(temp % 8, true);
      line_horz(temp / 8, true);
      refresh = true;
    }
    else if ((sensor0Val == LOW) && (mode == WAIT_FOR_SCALE)) // pressed the button twice
    {
      // Turn off column and row for a beat to indicate current selection
      uint8_t temp = scale_notes[layer][visible_grid_set];

      if (temp > MAX_SCALES)
      {
        temp = MAX_SCALES - 1;    
      }

      line_vert(temp % 8, false);
      line_horz(temp / 8, false);

      // Turn off column and row for a beat to indicate current selection
      temp = channels[layer][visible_grid_set];

      line_vert(temp % 8, true);
      line_horz(temp / 8, true);
      refresh = true;

      mode = WAIT_FOR_CHANNEL;
    }
    else if ((sensor0Val == LOW) && (mode == WAIT_FOR_CHANNEL)) // pressed the button twice
    {
      // Turn off column and row for a beat to indicate current selection
      uint8_t temp = channels[layer][visible_grid_set];

      line_vert(temp % 8, false);
      line_horz(temp / 8, false);
      refresh = true;

      mode = PLAYING;
    }  
  }
  lastSensor0Val =sensor0Val;

  // grid width select
  sensor13Val = digitalRead(BUTTON_WIDTH);
  if (sensor13Val != lastSensor13Val)
  {
    if ((sensor13Val == LOW) && (mode == PLAYING))
    {
      mode = WAIT_FOR_WIDTH;

      // Turn on column and row for a beat to indicate current selection
      uint8_t temp = grid_width[visible_grid_set] - 1;

      line_vert(temp % 8, true);
      line_horz(temp / 8, true);
      refresh = true;
    }
    else if ((sensor13Val == LOW) && (mode == WAIT_FOR_WIDTH)) // pressed the button twice
    {
      // Turn off column and row for a beat to indicate current selection
      uint8_t temp = grid_width[visible_grid_set] - 1;

      line_vert(temp % 8, false);
      line_horz(temp / 8, false);
      refresh = true;

      mode = PLAYING;
    }  
  }
  lastSensor13Val = sensor13Val;


  // instrument select
  sensor1Val = digitalRead(BUTTON_PROGRAM_CHANGE);
  if (sensor1Val != lastSensor1Val)
  {
    if ((sensor1Val == LOW) && (mode == PLAYING))
    {
      mode = WAIT_FOR_PROGRAM_CHANGE;

      // Turn on column and row for a beat to indicate current selection
      uint8_t temp = instrument[layer][visible_grid_set];

      if (temp >= 64)
      {
        temp = temp - 64;
      }

      line_vert(temp % 8, true);
      line_horz(temp / 8, true);
      refresh = true;
    }
    else if ((sensor1Val == LOW) && (mode == WAIT_FOR_PROGRAM_CHANGE)) // pressed the button twice
    {
      mode = WAIT_FOR_PROGRAM_CHANGE_2;
    }
    else if ((sensor1Val == LOW) && (mode == WAIT_FOR_PROGRAM_CHANGE_2)) // pressed the button twice
    {
      // Turn off column and row for a beat to indicate current selection
      uint8_t temp = instrument[layer][visible_grid_set];

      if (temp >= 64)
      {
        temp = temp - 64;
      }

      line_vert(temp % 8, false);
      line_horz(temp / 8, false);
      refresh = true;

      mode = PLAYING;
    }
  }
  lastSensor1Val = sensor1Val;

  // layer select
  sensor12Val = digitalRead(BUTTON_LAYER);
  if (sensor12Val != lastSensor12Val)
  {
    if ((sensor12Val == LOW) && (mode == PLAYING))
    {
      mode = WAIT_FOR_LAYER;

      // Turn on row to indicate current selection
      line_horz(layer, true);
      refresh = true;
    }
    else if ((sensor12Val == LOW) && (mode == WAIT_FOR_LAYER)) // pressed the button twice
    {
      // Turn off row to indicate current selection
      line_horz(layer, false);

      // Turn on row to indicate current selection
      line_vert(visible_grid_set, true);
      refresh = true;

      mode = WAIT_FOR_GRID;
    }
    else if ((sensor12Val == LOW) && (mode == WAIT_FOR_GRID)) // pressed the button twice
    {
      // Turn off row to indicate current selection
      line_vert(visible_grid_set, false);

      mode = PLAYING;
    }
  }
  lastSensor12Val = sensor12Val;

  // Turn on new column
  line_vert(col - display_offset, true);

  if (refresh == true) 
  {
    untztrument.writeDisplay();
  }

  while(usbMIDI.read()); // Discard incoming MIDI messages, todo pull in note on and note off to light lites on performance layer
}




// todo stuff for performance layer
/*
void OnNoteOn(byte channel, byte note, byte velocity)
 {
 note = note - LOWNOTE;
 untztrument.setLED(untztrument.xy2i(note % WIDTH, note / WIDTH));
 }
 
 void OnNoteOff(byte channel, byte note, byte velocity)
 {
 note = note - LOWNOTE;
 untztrument.clrLED(untztrument.xy2i(note % WIDTH, note / WIDTH));
 }
 
 void setup() {
 pinMode(LED, OUTPUT);
 #ifndef HELLA
 untztrument.begin(addr[0], addr[1], addr[2], addr[3]);
 #else
 untztrument.begin(addr[0], addr[1], addr[2], addr[3],
 addr[4], addr[5], addr[6], addr[7]);
 #endif // HELLA
 #ifdef __AVR__
 // Default Arduino I2C speed is 100 KHz, but the HT16K33 supports
 // 400 KHz.  We can force this for faster read & refresh, but may
 // break compatibility with other I2C devices...so be prepared to
 // comment this out, or save & restore value as needed.
 TWBR = 12;
 #endif
 untztrument.clear();
 untztrument.writeDisplay();
 usbMIDI.setHandleNoteOff(OnNoteOff);
 usbMIDI.setHandleNoteOn(OnNoteOn);
 }
 
 void loop() {
 unsigned long t = millis();
 if((t - prevReadTime) >= 20L) { // 20ms = min Trellis poll time
 if(untztrument.readSwitches()) { // Button state change?
 for(uint8_t i=0; i<N_BUTTONS; i++) { // For each button...
 // Get column/row for button, convert to MIDI note number
 uint8_t x, y, note;
 untztrument.i2xy(i, &x, &y);
 note = LOWNOTE + y * WIDTH + x;
 if(untztrument.justPressed(i)) {
 usbMIDI.sendNoteOn(note, 127, CHANNEL);
 //untztrument.setLED(i);
 } else if(untztrument.justReleased(i)) {
 usbMIDI.sendNoteOff(note, 0, CHANNEL);
 //untztrument.clrLED(i);
 }
 }
 
 }
 prevReadTime = t;
 digitalWrite(LED, ++heart & 32); // Blink = alive
 
 usbMIDI.read(2);
 untztrument.writeDisplay();
 }
 while(); // Discard incoming MIDI messages
 }
 */





