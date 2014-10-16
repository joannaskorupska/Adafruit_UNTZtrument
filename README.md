Adafruit_UNTZtrument
====================

The demo step sequencer contains modifications. It uses 4 encoders with push switches for additional input.

1. implements up to 16 beats on the grid with only an 8x8 untz using one of the encoders for scrolling left and right
2. the number of beats on the grid is adjustable up to 16
3. implements up to 8 layers outputing notes on different channels at the same time
4. implements up to 8 sets of sequence grid sets with 8 layers each that can be swiched
5. can change to and instruments (press button for first set 1-64, press again for second set 65-127)
6. has embedded notes scales (major, minor, chromatic, etc)
7. can adjust tempo and volume using 2 of the encoders
8. can adjust position on the scale with the last encoder
9. saves current state of the sequencer in EEPROM when button is held down for 10 seconds
