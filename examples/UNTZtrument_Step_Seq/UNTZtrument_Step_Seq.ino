// Simple UNTZtrument MIDI step sequencer.
// Requires an Arduino Leonardo w/TeeOnArdu config (or a PJRC Teensy),
// software on host computer for synth or to route to other devices.


// todos
// add C's as a horizontal line when scrolling up and down the scale
// allow transpositions between scales
// add measure marks as vertical lines when scrolling left and right
// allow selection of measure size and number of measures by making a box
// accept midi input to set the lights on the performance layer or just set and clear the lights ourself if no one is send us anything
// add other modes for games like lights out and tetris and break out and match two cards
// add built in simple synth for audio output, best we can do with space available, add speaker and holes and headphone jack
// add deluxe synth board and hook it up with internal chargable battery and speaker and headphone jack
// add flicker to UI display indicators to vertical and horizontal bars so they are distict from the notes
// add flicker from notes on other layers when playing notes, maybe not as much as the tanori
// add computer generated music or accompiment
// add blutooth audio output
// support less than 8 wide grid when scrolling and disable keys on right side
// add a playlist to play complete song based on layer sets, requires measures to have the same width
// need to check if long notes still work when changing button grid marks or layer and other switches
// need a scale with just the mt32 drumkit notes
// flash notes on other layers as they are played so we can see where the notes are being played on other layers
// add load/save state by midi note export and recording on an external device
// maybe hide play bar when scrolling to avoid confusion
// add an SD card reader to load and save multiple sets and larger sets and remove some of the restrictions that we have for the EEPROM saving
// switch grids on measure bars so you you can change earlier than at the end of a grid, less waiting.
// Add some multi colored LED underneath to dance to the music
// add eeprom write progress bar

#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_Trellis.h>
#include <Adafruit_UNTZtrument.h>
#include <Fluxamasynth.h>
#include <PgmChange.h>

Fluxamasynth synth;

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
enc eScaleOffset(5, A4);
enc eDisplayOffset(9, 8);
enc eTempo(7, 6);
enc eVolume(11, 10);

#define BUTTON_WIDTH            A5
#define BUTTON_GRID             0
#define BUTTON_PROGRAM_CHANGE   1
#define BUTTON_LAYER            12

uint8_t       col          = 0;            // Current column
unsigned long prevBeatTime = 0L;           // Column step timer
unsigned long prevReadTime = 0L;           // Keypad polling timer
long longButtonPressTime = -1;

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

uint8_t       grid[GRID_WIDTH][LAYERS - 1][GRID_SETS]; // bitfield. last layer is performance laye so does not need storage

uint8_t       channels[LAYERS];  // 1 to 127, 1 bit free
uint8_t       long_notes[LAYERS] = {
  1, 1, 1, 1, 1, 1, 1, 0}; // 0 to 1, 7 bits free
uint8_t       noteOffset[LAYERS - 1][GRID_SETS]; // 0 to (MAX_MIDI_NOTES-BUTTON_GRID_WIDTH), 2 bits free, last layer is performance laye so does not need storage
uint8_t       volume[LAYERS]; // 0 to 127, 1 bit free
uint8_t       instrument[LAYERS]; // 1 to 127, 1 bit free
uint8_t       scale_notes[LAYERS]; // 0 to (MAX_SCALES-1), 4 bits free
// total of 16 bits not used, 128 bytes packed

uint8_t       grid_width[GRID_SETS];  // 0 to (GRID_WIDTH-1) 4 bits free
uint8_t       tempo[GRID_SETS];
unsigned long beatInterval[GRID_SETS]; // = 60000L / 240; // ms/beat default to 240 bpm

//uint8_t max_grid_width_table[14] = {
//  8, 9, 10, 12, 14, 15, 16, 20, 21, 24, 25, 27, 28, 30}; // mutiples of 2, 3, 4, 5, 6, 7 that are equal to or more than 8

#define MIDI_MIDDLE_C 60
#define MAX_MIDI_NOTES 56
#define MAX_SCALES 34

// these are stored in program memory so they do not take up ram and can be stored unpacked for quick easy access
// todo add total of 64 scales, since we have pleanty of program space and buttons
// add MT32 drum channel scale with only active notes

static const uint8_t PROGMEM
scales[MAX_SCALES][MAX_MIDI_NOTES] = {
  {
    107, 105, 103, 101, 100, 98, 96, 95, 93, 91, 89, 88, 86, 84, 83, 81, 79, 77, 76, 74, 72, 71, 69, 67, 65, 64, 62, 60, 59, 57, 55, 53, 52, 50, 48, 47, 45, 43, 41, 40, 38, 36, 35, 33, 31, 29, 28, 26, 24, 23, 21, 19, 17, 16, 14, 12                } 
  ,
  {
    106, 104, 103, 101, 99, 98, 96, 94, 92, 91, 89, 87, 86, 84, 82, 80, 79, 77, 75, 74, 72, 70, 68, 67, 65, 63, 62, 60, 58, 56, 55, 53, 51, 50, 48, 46, 44, 43, 41, 39, 38, 36, 34, 32, 31, 29, 27, 26, 24, 22, 20, 19, 17, 15, 14, 12                } 
  ,
  {
    87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32                } 
  ,
  {
    114, 113, 111, 108, 106, 103, 102, 101, 99, 96, 94, 91, 90, 89, 87, 84, 82, 79, 78, 77, 75, 72, 70, 67, 66, 65, 63, 60, 58, 55, 54, 53, 51, 48, 46, 43, 42, 41, 39, 36, 34, 31, 30, 29, 27, 24, 22, 19, 18, 17, 15, 12, 10, 7, 6, 5                } 
  ,
  {
    100, 99, 97, 96, 94, 93, 91, 90, 88, 87, 85, 84, 82, 81, 79, 78, 76, 75, 73, 72, 70, 69, 67, 66, 64, 63, 61, 60, 58, 57, 55, 54, 52, 51, 49, 48, 46, 45, 43, 42, 40, 39, 37, 36, 34, 33, 31, 30, 28, 27, 25, 24, 22, 21, 19, 18                } 
  ,
  {
    96, 95, 94, 93, 92, 91, 89, 87, 86, 84, 83, 82, 81, 80, 79, 77, 75, 74, 72, 71, 70, 69, 68, 67, 65, 63, 62, 60, 59, 58, 57, 56, 55, 53, 51, 50, 48, 47, 46, 45, 44, 43, 41, 39, 38, 36, 35, 34, 33, 32, 31, 29, 27, 26, 24, 23                } 
  ,
  {
    107, 104, 103, 101, 100, 98, 96, 95, 92, 91, 89, 88, 86, 84, 83, 80, 79, 77, 76, 74, 72, 71, 68, 67, 65, 64, 62, 60, 59, 56, 55, 53, 52, 50, 48, 47, 44, 43, 41, 40, 38, 36, 35, 32, 31, 29, 28, 26, 24, 23, 20, 19, 17, 16, 14, 12                } 
  ,
  {
    107, 104, 103, 101, 99, 98, 96, 95, 92, 91, 89, 87, 86, 84, 83, 80, 79, 77, 75, 74, 72, 71, 68, 67, 65, 63, 62, 60, 59, 56, 55, 53, 51, 50, 48, 47, 44, 43, 41, 39, 38, 36, 35, 32, 31, 29, 27, 26, 24, 23, 20, 19, 17, 15, 14, 12                } 
  ,
  {
    107, 105, 103, 101, 99, 98, 96, 95, 93, 91, 89, 87, 86, 84, 83, 81, 79, 77, 75, 74, 72, 71, 69, 67, 65, 63, 62, 60, 59, 57, 55, 53, 51, 50, 48, 47, 45, 43, 41, 39, 38, 36, 35, 33, 31, 29, 27, 26, 24, 23, 21, 19, 17, 15, 14, 12                } 
  ,
  {
    115, 111, 110, 108, 107, 105, 103, 99, 98, 96, 95, 93, 91, 87, 86, 84, 83, 81, 79, 75, 74, 72, 71, 69, 67, 63, 62, 60, 59, 57, 55, 51, 50, 48, 47, 45, 43, 39, 38, 36, 35, 33, 31, 27, 26, 24, 23, 21, 19, 15, 14, 12, 11, 9, 7, 3                } 
  ,
  {
    106, 105, 102, 101, 100, 97, 96, 94, 93, 90, 89, 88, 85, 84, 82, 81, 78, 77, 76, 73, 72, 70, 69, 66, 65, 64, 61, 60, 58, 57, 54, 53, 52, 49, 48, 46, 45, 42, 41, 40, 37, 36, 34, 33, 30, 29, 28, 25, 24, 22, 21, 18, 17, 16, 13, 12                } 
  ,
  {
    106, 103, 102, 101, 100, 98, 96, 94, 91, 90, 89, 88, 86, 84, 82, 79, 78, 77, 76, 74, 72, 70, 67, 66, 65, 64, 62, 60, 58, 55, 54, 53, 52, 50, 48, 46, 43, 42, 41, 40, 38, 36, 34, 31, 30, 29, 28, 26, 24, 22, 19, 18, 17, 16, 14, 12                } 
  ,
  {
    96, 95, 93, 92, 91, 89, 88, 87, 85, 84, 83, 81, 80, 79, 77, 76, 75, 73, 72, 71, 69, 68, 67, 65, 64, 63, 61, 60, 59, 57, 56, 55, 53, 52, 51, 49, 48, 47, 45, 44, 43, 41, 40, 39, 37, 36, 35, 33, 32, 31, 29, 28, 27, 25, 24, 23                } 
  ,
  {
    106, 105, 103, 101, 99, 98, 96, 94, 93, 91, 89, 87, 86, 84, 82, 81, 79, 77, 75, 74, 72, 70, 69, 67, 65, 63, 62, 60, 58, 57, 55, 53, 51, 50, 48, 46, 45, 43, 41, 39, 38, 36, 34, 33, 31, 29, 27, 26, 24, 22, 21, 19, 17, 15, 14, 12                } 
  ,
  {
    106, 104, 101, 100, 97, 97, 96, 94, 92, 89, 88, 85, 85, 84, 82, 80, 77, 76, 73, 73, 72, 70, 68, 65, 64, 61, 61, 60, 58, 56, 53, 52, 49, 49, 48, 46, 44, 41, 40, 37, 37, 36, 34, 32, 29, 28, 25, 25, 24, 22, 20, 17, 16, 13, 13, 12                } 
  ,
  {
    106, 104, 102, 101, 99, 97, 96, 94, 92, 90, 89, 87, 85, 84, 82, 80, 78, 77, 75, 73, 72, 70, 68, 66, 65, 63, 61, 60, 58, 56, 54, 53, 51, 49, 48, 46, 44, 42, 41, 39, 37, 36, 34, 32, 30, 29, 27, 25, 24, 22, 20, 18, 17, 15, 13, 12                } 
  ,
  {
    106, 105, 103, 102, 100, 98, 96, 94, 93, 91, 90, 88, 86, 84, 82, 81, 79, 78, 76, 74, 72, 70, 69, 67, 66, 64, 62, 60, 58, 57, 55, 54, 52, 50, 48, 46, 45, 43, 42, 40, 38, 36, 34, 33, 31, 30, 28, 26, 24, 22, 21, 19, 18, 16, 14, 12                } 
  ,
  {
    96, 95, 94, 93, 92, 91, 89, 87, 86, 84, 83, 82, 81, 80, 79, 77, 75, 74, 72, 71, 70, 69, 68, 67, 65, 63, 62, 60, 59, 58, 57, 56, 55, 53, 51, 50, 48, 47, 46, 45, 44, 43, 41, 39, 38, 36, 35, 34, 33, 32, 31, 29, 27, 26, 24, 23                } 
  ,
  {
    106, 105, 103, 101, 100, 98, 96, 94, 93, 91, 89, 88, 86, 84, 82, 81, 79, 77, 76, 74, 72, 70, 69, 67, 65, 64, 62, 60, 58, 57, 55, 53, 52, 50, 48, 46, 45, 43, 41, 40, 38, 36, 34, 33, 31, 29, 28, 26, 24, 22, 21, 19, 17, 16, 14, 12                } 
  ,
  {
    124, 122, 120, 117, 115, 112, 110, 108, 105, 103, 100, 98, 96, 93, 91, 88, 86, 84, 81, 79, 76, 74, 72, 69, 67, 64, 62, 60, 57, 55, 52, 50, 48, 45, 43, 40, 38, 36, 33, 31, 28, 26, 24, 21, 19, 16, 14, 12, 9, 7, 4, 2, 0, 253, 251, 248                } 
  ,
  {
    106, 104, 103, 101, 99, 97, 96, 94, 92, 91, 89, 87, 85, 84, 82, 80, 79, 77, 75, 73, 72, 70, 68, 67, 65, 63, 61, 60, 58, 56, 55, 53, 51, 49, 48, 46, 44, 43, 41, 39, 37, 36, 34, 32, 31, 29, 27, 25, 24, 22, 20, 19, 17, 15, 13, 12                } 
  ,
  {
    107, 106, 103, 101, 99, 97, 96, 95, 94, 91, 89, 87, 85, 84, 83, 82, 79, 77, 75, 73, 72, 71, 70, 67, 65, 63, 61, 60, 59, 58, 55, 53, 51, 49, 48, 47, 46, 43, 41, 39, 37, 36, 35, 34, 31, 29, 27, 25, 24, 23, 22, 19, 17, 15, 13, 12                } 
  ,
  {
    96, 95, 94, 93, 91, 89, 88, 87, 86, 84, 83, 82, 81, 79, 77, 76, 75, 74, 72, 71, 70, 69, 67, 65, 64, 63, 62, 60, 59, 58, 57, 55, 53, 52, 51, 50, 48, 47, 46, 45, 43, 41, 40, 39, 38, 36, 35, 34, 33, 31, 29, 28, 27, 26, 24, 23                } 
  ,
  {
    114, 112, 110, 108, 106, 104, 102, 100, 98, 96, 94, 92, 90, 88, 86, 84, 82, 80, 78, 76, 74, 72, 70, 68, 66, 64, 62, 60, 58, 56, 54, 52, 50, 48, 46, 44, 42, 40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10, 8, 6, 4                } 
  ,
  {
    100, 99, 97, 96, 94, 92, 90, 89, 88, 87, 85, 84, 82, 80, 78, 77, 76, 75, 73, 72, 70, 68, 66, 65, 64, 63, 61, 60, 58, 56, 54, 53, 52, 51, 49, 48, 46, 44, 42, 41, 40, 39, 37, 36, 34, 32, 30, 29, 28, 27, 25, 24, 22, 20, 18, 17                } 
  ,
  {
    112, 111, 109, 108, 104, 103, 100, 99, 97, 96, 92, 91, 88, 87, 85, 84, 80, 79, 76, 75, 73, 72, 68, 67, 64, 63, 61, 60, 56, 55, 52, 51, 49, 48, 44, 43, 40, 39, 37, 36, 32, 31, 28, 27, 25, 24, 20, 19, 16, 15, 13, 12, 8, 7, 4, 3                } 
  ,
  {
    123, 122, 120, 117, 115, 111, 110, 108, 105, 103, 99, 98, 96, 93, 91, 87, 86, 84, 81, 79, 75, 74, 72, 69, 67, 63, 62, 60, 57, 55, 51, 50, 48, 45, 43, 39, 38, 36, 33, 31, 27, 26, 24, 21, 19, 15, 14, 12, 9, 7, 3, 2, 0, 253, 251, 247                } 
  ,
  {
    125, 121, 120, 118, 114, 113, 109, 108, 106, 102, 101, 97, 96, 94, 90, 89, 85, 84, 82, 78, 77, 73, 72, 70, 66, 65, 61, 60, 58, 54, 53, 49, 48, 46, 42, 41, 37, 36, 34, 30, 29, 25, 24, 22, 18, 17, 13, 12, 10, 6, 5, 1, 0, 254, 250, 249                } 
  ,
  {
    125, 121, 120, 118, 115, 113, 109, 108, 106, 103, 101, 97, 96, 94, 91, 89, 85, 84, 82, 79, 77, 73, 72, 70, 67, 65, 61, 60, 58, 55, 53, 49, 48, 46, 43, 41, 37, 36, 34, 31, 29, 25, 24, 22, 19, 17, 13, 12, 10, 7, 5, 1, 0, 254, 251, 249                } 
  ,
  {
    123, 122, 120, 116, 115, 111, 110, 108, 104, 103, 99, 98, 96, 92, 91, 87, 86, 84, 80, 79, 75, 74, 72, 68, 67, 63, 62, 60, 56, 55, 51, 50, 48, 44, 43, 39, 38, 36, 32, 31, 27, 26, 24, 20, 19, 15, 14, 12, 8, 7, 3, 2, 0, 252, 251, 247                } 
  ,
  {
    107, 104, 103, 102, 99, 98, 96, 95, 92, 91, 90, 87, 86, 84, 83, 80, 79, 78, 75, 74, 72, 71, 68, 67, 66, 63, 62, 60, 59, 56, 55, 54, 51, 50, 48, 47, 44, 43, 42, 39, 38, 36, 35, 32, 31, 30, 27, 26, 24, 23, 20, 19, 18, 15, 14, 12                } 
  ,
  {
    107, 104, 103, 101, 100, 97, 96, 95, 92, 91, 89, 88, 85, 84, 83, 80, 79, 77, 76, 73, 72, 71, 68, 67, 65, 64, 61, 60, 59, 56, 55, 53, 52, 49, 48, 47, 44, 43, 41, 40, 37, 36, 35, 32, 31, 29, 28, 25, 24, 23, 20, 19, 17, 16, 13, 12                } 
  ,
  {
    106, 104, 102, 100, 99, 97, 96, 94, 92, 90, 88, 87, 85, 84, 82, 80, 78, 76, 75, 73, 72, 70, 68, 66, 64, 63, 61, 60, 58, 56, 54, 52, 51, 49, 48, 46, 44, 42, 40, 39, 37, 36, 34, 32, 30, 28, 27, 25, 24, 22, 20, 18, 16, 15, 13, 12                } 
  ,
  {
    125, 123, 120, 118, 115, 113, 111, 108, 106, 103, 101, 99, 96, 94, 91, 89, 87, 84, 82, 79, 77, 75, 72, 70, 67, 65, 63, 60, 58, 55, 53, 51, 48, 46, 43, 41, 39, 36, 34, 31, 29, 27, 24, 22, 19, 17, 15, 12, 10, 7, 5, 3, 0, 254, 251, 249                } 
};

/*
// could store these at 16 bit numbers with byte length embedded in the top 4 bits
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
  // write a magic number and a format version number
  EEPROM.write(address + 0, 0xd5); // magic number d6aa96
  EEPROM.write(address + 1, 0xaa);
  EEPROM.write(address + 2, 0x96);
  EEPROM.write(address + 3, 0x03); // version 3
  address += 4;
  saveToEEPROM(address, (uint8_t *)&grid[0], sizeof(grid));
  address += sizeof(grid);
  saveToEEPROM(address, (uint8_t *)&grid_width[0], sizeof(grid_width));
  address += sizeof(grid_width);
  saveToEEPROM(address, (uint8_t *)&noteOffset[0], sizeof(noteOffset));
  address += sizeof(noteOffset);
  saveToEEPROM(address, (uint8_t *)&long_notes[0], sizeof(long_notes));
  address += sizeof(long_notes);
  saveToEEPROM(address, (uint8_t *)&volume[0], sizeof(volume));
  address += sizeof(volume);
  saveToEEPROM(address, (uint8_t *)&instrument[0], sizeof(instrument));
  address += sizeof(instrument);
  saveToEEPROM(address, (uint8_t *)&scale_notes[0], sizeof(scale_notes));
  address += sizeof(scale_notes);
  saveToEEPROM(address, (uint8_t *)&tempo[0], sizeof(tempo));
  address += sizeof(tempo);
  saveToEEPROM(address, (uint8_t *)&channels[0], sizeof(channels));
  address += sizeof(channels);
  saveToEEPROM(address, (uint8_t *)&playing_grid_set, sizeof(playing_grid_set));
  address += sizeof(playing_grid_set);
  saveToEEPROM(address, (uint8_t *)&layer, sizeof(layer));
  address += sizeof(layer);
  saveToEEPROM(address, (uint8_t *)&display_offset, sizeof(display_offset));
  address += sizeof(display_offset);

  //1015 bytes it fits, barely
}

void load(void)
{
  int address = 0;

  // check for a magic number and a format version number
  if ((EEPROM.read(address + 0) == 0xd5) && // magic number d6aa96
  (EEPROM.read(address + 1) == 0xaa) &&
    (EEPROM.read(address + 2) == 0x96) &&
    (EEPROM.read(address + 3) == 0x03)) // version 3
  {
    address += 4;
    loadFromEEPROM(address, (uint8_t *)&grid[0], sizeof(grid));
    address += sizeof(grid);
    loadFromEEPROM(address, (uint8_t *)&grid_width[0], sizeof(grid_width));
    address += sizeof(grid_width);
    loadFromEEPROM(address, (uint8_t *)&noteOffset[0], sizeof(noteOffset));
    address += sizeof(noteOffset);
    loadFromEEPROM(address, (uint8_t *)&long_notes[0], sizeof(long_notes));
    address += sizeof(long_notes);
    loadFromEEPROM(address, (uint8_t *)&volume[0], sizeof(volume));
    address += sizeof(volume);
    loadFromEEPROM(address, (uint8_t *)&instrument[0], sizeof(instrument));
    address += sizeof(instrument);
    loadFromEEPROM(address, (uint8_t *)&scale_notes[0], sizeof(scale_notes));
    address += sizeof(scale_notes);
    loadFromEEPROM(address, (uint8_t *)&tempo[0], sizeof(tempo));
    address += sizeof(tempo);
    loadFromEEPROM(address, (uint8_t *)&channels[0], sizeof(channels));
    address += sizeof(channels);
    loadFromEEPROM(address, (uint8_t *)&playing_grid_set, sizeof(playing_grid_set));
    visible_grid_set = playing_grid_set;
    address += sizeof(playing_grid_set);
    loadFromEEPROM(address, (uint8_t *)&layer, sizeof(layer));
    address += sizeof(layer);
    loadFromEEPROM(address, (uint8_t *)&display_offset, sizeof(display_offset));
    address += sizeof(display_offset);
  }
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
  memset(long_notes, 0, sizeof(long_notes));  // default all layers to short notes
  memset(tempo, 240/4, sizeof(long_notes));  // default all grids to 240 bpm
  // todo make sure all channels are always different for all layers
  channels[0] = 3;
  channels[1] = 4;
  channels[2] = 5;
  channels[3] = 6;
  channels[4] = 7;
  channels[5] = 8;
  channels[6] = 9; // drum kit
  channels[7] = 2; // performance layer

  load(); // attempt to load from eeprom

  eScaleOffset.setBounds(0, 44 * 4 + 3); // Set note offset limits
  eScaleOffset.setValue(noteOffset[layer][visible_grid_set] * 4);              // *4's for encoder detents
  eTempo.setBounds(20 * 4, 1000 * 4 + 3); // Set tempo limits
  eTempo.setValue(tempo[visible_grid_set] * 16);              // *4's for encoder detents and *4 for byte storage
  eDisplayOffset.setBounds(0, (grid_width[visible_grid_set] - BUTTON_GRID_WIDTH) * 4 + 3); // Set diplay offset limits
  eDisplayOffset.setValue(0);              // *4's for encoder detents
  eVolume.setBounds(0, 127 * 4 + 3); // Set volume limits
  eVolume.setValue(volume[layer] * 4);              // *4's for encoder detents

  if (layer == LAYERS-1)
  {
    untztrument.setLED(untztrument.xy2i(7, visible_grid_set));
  }
  else
  {
    // redraw whole screen 
    for (uint8_t x = 0; x < 8; x++)
    {
      line_vert(x, false);
    }
  }

  // setup initial instruments and volumes on channels
  for (uint8_t layer_index = 0; layer_index < LAYERS; layer_index++)
  {
    uint8_t this_instrument = instrument[layer_index];
    uint8_t this_channel = channels[layer_index];
    uint8_t this_volume = volume[layer_index];
    usbMIDI.sendProgramChange(this_instrument, this_channel);
    synth.programChange(0, this_channel, this_instrument);
    usbMIDI.sendControlChange(7, this_volume, this_channel);
    synth.setChannelVolume(this_channel, this_volume);
    usbMIDI.sendControlChange(0x7b, 0, this_channel);
    synth.allNotesOff(this_channel);
  }

  for (uint8_t grid_index = 0; grid_index < GRID_SETS; grid_index++)
  {
    beatInterval[grid_index] = 15000L / tempo[grid_index];
  }
}

bool flicker_state = true;

// Turn on (or off) one column of the display
void line_vert(uint8_t x, boolean setit) 
{
  for(uint8_t mask=1, y=0; y<8; y++, mask <<= 1) 
  {
    uint8_t i = untztrument.xy2i(x, y);
    if (layer == LAYERS-1)
    {
      if (setit)
      {
        untztrument.setLED(i);
      }
      else          
      {
        untztrument.clrLED(i);
      }
    }
    else if((setit && flicker_state) || (grid[x + display_offset][layer][visible_grid_set] & mask)) 
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
    if (layer == LAYERS-1)
    {
      if (setit)
      {
        untztrument.setLED(i);
      }
      else          
      {
        untztrument.clrLED(i);
      }
    }
    else if((setit && flicker_state) || (grid[column_index + display_offset][layer][visible_grid_set] & mask)) 
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
            uint8_t temp = channels[layer];
            usbMIDI.sendNoteOff(pgm_read_byte(&scales[scale_notes[layer]][6-x + y*7]), 127, temp);
            synth.noteOff(temp, pgm_read_byte(&scales[scale_notes[layer]][6-x + y*7]), 127);
            untztrument.clrLED(i);

            refresh = true;
          }
        }

        if(untztrument.justPressed(i)) 
        { 
          switch (mode)
          {
          case PLAYING_ERASE_PROGRAM_CHANGE_1:
          case PLAYING_ERASE_PROGRAM_CHANGE_2:
          case PLAYING_ERASE_CHANNEL_1:
          case PLAYING_ERASE_CHANNEL_2:
          case PLAYING_ERASE_LAYER_1:
          case PLAYING_ERASE_LAYER_2:
          case PLAYING_ERASE_GRID_1:
          case PLAYING_ERASE_GRID_2:
          case PLAYING_ERASE_WIDTH_1:
          case PLAYING_ERASE_WIDTH_2:
          case PLAYING_ERASE_SCALE_1:
          case PLAYING_ERASE_SCALE_2:
          case PLAYING_ERASE_C_1:
          case PLAYING_ERASE_C_2:
          case PLAYING_ERASE_MEASURE_1:
          case PLAYING_ERASE_MEASURE_2:
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
                  uint8_t temp2 = channels[layer];
                  usbMIDI.sendNoteOff(pgm_read_byte(&scales[scale_notes[layer]][y + noteOffset[layer][playing_grid_set]]), 127, temp2);
                  synth.noteOff(temp2, pgm_read_byte(&scales[scale_notes[layer]][y + noteOffset[layer][playing_grid_set]]), 127);
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
                uint8_t temp = channels[layer];
                usbMIDI.sendNoteOn(pgm_read_byte(&scales[scale_notes[layer]][6-x + y*7] ) , 127, temp);
                synth.noteOn(temp, pgm_read_byte(&scales[scale_notes[layer]][6-x + y*7] ) , 127);
                untztrument.setLED(i);
              }
              else
              {
                untztrument.clrLED(untztrument.xy2i(7, visible_grid_set));
                visible_grid_set = y;
                eTempo.setValue(tempo[visible_grid_set] * 16);  // *4's for encoder detents and *4 for byte storage
                untztrument.setLED(i);
              }
            }
            refresh = true;
            break;

          case WAIT_FOR_PROGRAM_CHANGE:
            {
              uint8_t temp = instrument[layer];

              if (temp >= 64)
              {
                temp = temp - 64;  
              }

              // Turn off column and row
              line_vert(temp % 8, false);
              line_horz(temp / 8, false);

              temp = x+y*8;
              instrument[layer] = temp;

              usbMIDI.sendProgramChange(temp, channels[layer]);
              synth.programChange(0, channels[layer], temp);

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
              uint8_t temp = instrument[layer];
              if (temp >= 64)
              {
                temp = temp - 64;  
              }

              // Turn off column and row
              line_vert(temp % 8, false);
              line_horz(temp / 8, false);

              temp = x + y*8 + 64;
              instrument[layer] = temp;

              usbMIDI.sendProgramChange(temp, channels[layer]);
              synth.programChange(0, channels[layer], temp);
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
              if (layer != LAYERS -1 )
              {
                eScaleOffset.setValue(noteOffset[layer][visible_grid_set] * 4);              // *4's for encoder detents
              }
              eVolume.setValue(volume[layer] * 4);              // *4's for encoder detents

              refresh = true;
              break;
            }

          case WAIT_FOR_GRID:
            {
              // Turn off row
              line_vert(visible_grid_set, false);

              visible_grid_set = x;
              eTempo.setValue(tempo[visible_grid_set] * 16);  // *4's for encoder detents and *4 for byte storage

              mode = PLAYING_ERASE_GRID_1;

              // redraw whole screen because grid change
              for (uint8_t x = 0; x < 8; x++)
              {
                line_vert(x, false);
              }

              // adjust encoder ranges to match next grid set on current layer
              if (layer != LAYERS -1 )
              {
                eScaleOffset.setValue(noteOffset[layer][visible_grid_set] * 4);              // *4's for encoder detents
                eDisplayOffset.setBounds(0, (grid_width[visible_grid_set] - BUTTON_GRID_WIDTH) * 4 + 3); // Set diplay offset limits
                eDisplayOffset.setValue(0);              // *4's for encoder detents
              }
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
              if (layer != LAYERS -1 )
              {
                eDisplayOffset.setBounds(0, (temp - BUTTON_GRID_WIDTH) * 4 + 3); // Set diplay offset limits
                eDisplayOffset.setValue(0);              // *4's for encoder detents
              }

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
              uint8_t temp = scale_notes[layer];

              if (temp >= MAX_SCALES)
              {
                temp = MAX_SCALES - 1;  
              }

              // Turn off column and row
              line_vert(temp % 8, false);
              line_horz(temp / 8, false);

              temp = x+y*8;
              if (temp >= MAX_SCALES)
              {
                temp = MAX_SCALES - 1;  
              }

              scale_notes[layer] = temp;

              // flip the long note setting
              if (long_notes[layer] == 0)
              {
                long_notes[layer] = 1;              
              }
              else
              {
                long_notes[layer] = 0;           
              }

              // Send midi control change all notes off for that channel because new scale might not contain any of the old notes that are currently playing
              usbMIDI.sendControlChange(0x7b, 0, channels[layer]);
              synth.allNotesOff(channels[layer]);

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
              uint8_t temp = channels[layer];

              // Turn off column and row
              line_vert(temp % 8, false);
              line_horz(temp / 8, false);

              temp = x+y*8;
              channels[layer] = temp;

              // todo after channel change need to resend volume and instrument for new channel

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
  if (layer != LAYERS -1 )
  {
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
          grid[column_index][layer][visible_grid_set] <<= diff;
        }
        else
        {
          grid[column_index][layer][visible_grid_set] >>= abs(diff);
        }
      }

      // Send midi control change all notes off because we might have scrolled off playing notes
      usbMIDI.sendControlChange(0x7b, 0, channels[layer]);
      synth.allNotesOff(channels[layer]);

      // shift all notes and redraw columns
      for(uint8_t row_index=0; row_index < grid_width[visible_grid_set]; row_index++) 
      {
#if 0 //decided I don't like this
        if (((temp + row_index) % 7) == 0) //todo this needs to actually point to the C's in a scale (%7 for the moment), scales can have different sizes but Middle C is always placed in the center of the note scale. 
        {
          line_horz(row_index, true);
          //todo need to adjust encoder value back a notch when at limit so that we can detect when it moves further at the end of travel by looking at the undivided by 4 value
        }
        else
#endif
        {
          line_horz(row_index, false);
        }
      }

      mode = PLAYING_ERASE_C_1;
      refresh      = true;
    }
  }


  if (layer != LAYERS -1 )
  {
    // adjust the note offset here so all notes are off when we make the change
    int temp = eDisplayOffset.getValue() / 4;
    int diff = display_offset - temp;
    if (diff != 0)
    {
      display_offset = temp;

      // redraw all columns
      for(uint8_t column_index=0; column_index < BUTTON_GRID_WIDTH; column_index++) 
      {
#if 0 // decided I don't like this
        if (((temp + column_index) % 4) == 0) //todo hard coded 4 beats a measure for indicator, need to make configured by grid width setting
        {
          line_vert(column_index, true);
          //todo need to adjust encoder value back a notch when at limit so that we can detect when it moves further at the end of travel by looking at the undivided by 4 value
        }
        else
#endif
        {
          line_vert(column_index, false);
        }
      }

      mode = PLAYING_ERASE_MEASURE_1;
      refresh      = true;
    }
  }

  if((t - prevBeatTime) >= beatInterval[playing_grid_set]) // Next beat?
  {
    // don't draw the progress bar on the performance layer
    if (layer != LAYERS - 1)
    {
      // Turn off old column
      line_vert(col - display_offset, false);
    }
    // stop playing notes on old column on all layers
    for(uint8_t layer_index=0;layer_index<LAYERS-1; layer_index++) 
    {
      for(uint8_t row=0, mask=1; row<8; row++, mask <<= 1) 
      {
        uint8_t this_grid_width = grid_width[playing_grid_set];
        uint8_t this_note = grid[col                        ][layer_index][playing_grid_set] & mask;
        uint8_t next_note = grid[(col + 1) % this_grid_width][layer_index][playing_grid_set] & mask;
        uint8_t about_to_switch_grids = ((col == (grid_width[playing_grid_set] - 1)) && (playing_grid_set != visible_grid_set));
        if(this_note && (!long_notes[layer_index] || (!next_note || about_to_switch_grids)))
        {
          uint8_t temp = channels[layer_index];
          uint8_t note = row + noteOffset[layer_index][playing_grid_set];
          uint8_t midinote = pgm_read_byte(&scales[scale_notes[layer_index]][note]);
          usbMIDI.sendNoteOff(midinote, 127, temp);
          synth.noteOff(temp, midinote, 127);

          if (layer == LAYERS -1)
          {
            untztrument.clrLED(untztrument.xy2i((MAX_MIDI_NOTES - note - 1) % 7, (note) / 7));
          }
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
    int temp = eVolume.getValue() / 4;
    int diff = volume[layer] - temp;
    if (diff != 0)
    {
      volume[layer] = temp;
      // send change volume on channels
      usbMIDI.sendControlChange(7, temp, channels[layer]);
      synth.setChannelVolume(channels[layer], temp);
    }

    // don't draw the prgress bar on the performance layer
    if (layer != LAYERS - 1)
    {
      // Turn on new column
      line_vert(col - display_offset, true);
    }
    // play notes on new column on all layers
    for(uint8_t layer_index=0;layer_index<LAYERS - 1; layer_index++) 
    {
      for (uint8_t row=0, mask=1; row<8; row++, mask <<= 1) 
      {
        uint8_t this_grid_width = grid_width[playing_grid_set];
        uint8_t this_note = grid[col                                                       ][layer_index][playing_grid_set] & mask;
        uint8_t last_note = grid[(col + grid_width[playing_grid_set] - 1) % this_grid_width][layer_index][playing_grid_set] & mask;
        uint8_t about_to_switch_grids = ((col == (grid_width[playing_grid_set] - 1)) && (playing_grid_set != visible_grid_set));

        if (this_note && (!long_notes[layer_index] || (!last_note || force_first_note)))
        {
          uint8_t temp = channels[layer_index];
          uint8_t note = row + noteOffset[layer_index][playing_grid_set];
          uint8_t midinote = pgm_read_byte(&scales[scale_notes[layer_index]][note]);
          usbMIDI.sendNoteOn(midinote, 127, temp);
          synth.noteOn(temp, midinote, 127);
          force_first_note = false; // clear trigger if it was set

          if (layer == LAYERS -1)
          {
            untztrument.setLED(untztrument.xy2i((MAX_MIDI_NOTES - note - 1) % 7, (note) / 7));
          }
        }
      }
    }

    prevBeatTime = t;
    refresh      = true;

    // todo add display of currnet tempo with a horizontal bar, possibley show other layer tempos
    int temp2 = eTempo.getValue() / 4; // Div4 for encoder detents
    tempo[visible_grid_set] = temp2 / 4 ; // Div4 byte storage
    beatInterval[visible_grid_set] = 60000L / temp2;
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
      uint8_t temp = instrument[layer];

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
      uint8_t temp = scale_notes[layer];

      if (temp >= MAX_SCALES)
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
      uint8_t temp = channels[layer];
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
      uint8_t temp = instrument[layer];

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
      uint8_t temp = scale_notes[layer];

      if (temp >= MAX_SCALES)
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
      uint8_t temp = channels[layer];

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

  // todo make first button clear current layer, second clear current grid, third clear everything

  // scale select
  sensor0Val = digitalRead(BUTTON_GRID);
  if (sensor0Val != lastSensor0Val)
  {
    longButtonPressTime = t;
    if ((sensor0Val == LOW) && (mode == PLAYING))
    {
      mode = WAIT_FOR_SCALE;

      // Turn on row for a beat to indicate current selection
      uint8_t temp = scale_notes[layer];

      if (temp >= MAX_SCALES)
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
      uint8_t temp = scale_notes[layer];

      if (temp >= MAX_SCALES)
      {
        temp = MAX_SCALES - 1;    
      }

      line_vert(temp % 8, false);
      line_horz(temp / 8, false);

      // Turn off column and row for a beat to indicate current selection
      temp = channels[layer];

      line_vert(temp % 8, true);
      line_horz(temp / 8, true);
      refresh = true;

      mode = WAIT_FOR_CHANNEL;
    }
    else if ((sensor0Val == LOW) && (mode == WAIT_FOR_CHANNEL)) // pressed the button twice
    {
      // Turn off column and row for a beat to indicate current selection
      uint8_t temp = channels[layer];

      line_vert(temp % 8, false);
      line_horz(temp / 8, false);
      refresh = true;

      mode = PLAYING;
    }  
  }
  else if ((t - longButtonPressTime > 5000) && (lastSensor0Val == LOW))
  {
    longButtonPressTime = millis();

  // stop current stuff
  for (uint8_t layer_index = 0; layer_index < LAYERS; layer_index++)
  {
    // Send midi control change all notes off
    usbMIDI.sendControlChange(0x7b, 0, channels[layer_index]);
    synth.allNotesOff(channels[layer_index]);
  }
  
    // clear layers
    untztrument.clear();
    untztrument.writeDisplay();
    refresh = true;

    memset(grid, 0, sizeof(grid)); // no notes set yet, all grid sets and layers
    memset(noteOffset, 23, sizeof(noteOffset)); // current center of note scale for each layer and grid set
    memset(volume, 64, sizeof(volume)); // 1/2 range current midi volume for each layer
    memset(instrument, 0, sizeof(instrument)); // current midi instrument for each layer
    memset(grid_width, 16, sizeof(grid_width)); // default all grids to 16 beats wide
    memset(scale_notes, 0, sizeof(scale_notes)); // default all grids to major scale
    memset(channels, 2, sizeof(channels)); // default all channels to 2
    memset(long_notes, 0, sizeof(long_notes));  // default all layers to short notes
    memset(tempo, 240/4, sizeof(long_notes));  // default all grids to 240 bpm
    // todo make sure all channels are always different for all layers
    channels[0] = 3;
    channels[1] = 4;
    channels[2] = 5;
    channels[3] = 6;
    channels[4] = 7;
    channels[5] = 8;
    channels[6] = 9; // drum kit
    channels[7] = 2; // performance layer
    
  // setup initial instruments and volumes on channels
  for (uint8_t layer_index = 0; layer_index < LAYERS; layer_index++)
  {
    uint8_t this_instrument = instrument[layer_index];
    uint8_t this_channel = channels[layer_index];
    uint8_t this_volume = volume[layer_index];
    usbMIDI.sendProgramChange(this_instrument, this_channel);
    synth.programChange(0, this_channel, this_instrument);
    usbMIDI.sendControlChange(7, this_volume, this_channel);
    synth.setChannelVolume(this_channel, this_volume);
    usbMIDI.sendControlChange(0x7b, 0, this_channel);
    synth.allNotesOff(this_channel);
  }
      mode = PLAYING;

  }
  lastSensor0Val =sensor0Val;

  // grid width select
  sensor13Val = digitalRead(BUTTON_WIDTH);
  if (sensor13Val != lastSensor13Val)
  {
    longButtonPressTime = t;
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
  else if ((t - longButtonPressTime > 5000) && (sensor13Val == LOW))
  {
    longButtonPressTime = millis();
    //line_horz(1, true);
    save();  // todo need to do this only once and make it stop afterwards, if you hold the button down it repeats the save
    
    mode = PLAYING_ERASE_WIDTH_2;
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
      uint8_t temp = instrument[layer];

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
      uint8_t temp = instrument[layer];

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

  // draw grid indicator on performance layer
  if (layer == LAYERS - 1)
  {
    untztrument.setLED(untztrument.xy2i(7, visible_grid_set));
  }
  // draw the progress bar on other layers
  else 
  {
    // Turn on new column
    line_vert(col - display_offset, true);
  }


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

















