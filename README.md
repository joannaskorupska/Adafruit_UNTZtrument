Adafruit_UNTZtrument
====================

The demo step sequencer contains modifications. It uses 4 encoders with push switches for additional input.

![untz step sequencer](/../master/basic.gif?raw=true)

1. implements up to 16 beats on the grid with only an 8x8 untz using one of the encoders for scrolling left and right
2. the number of beats on the grid is adjustable up to 16
3. implements up to 8 layers outputing notes on different channels at the same time
4. implements up to 8 sets of sequence grid sets with 8 layers each that can be swiched
5. can change to and instruments (press button for first set 1-64, press again for second set 65-127)
6. has embedded notes scales (major, minor, chromatic, etc)
7. can adjust tempo and volume using 2 of the encoders
8. can adjust position on the scale with the last encoder
9. saves current state of the sequencer in EEPROM when button is held down for 10 seconds

![untz step sequencer](/../master/button1.gif?raw=true)

Press button 1 to select the scale, press button 1 again to select the midi channel, press it again to return to normal. The value is indicated by the location of the intersection of the displayed horizontal and vertical line counting left to right and down. The value is changed by selecting a new value while the current value is being displayed. Selection of the same scale will alternate between notes being held for a duration and playing the note for each sequence light that is lite.

Hold button 1 for five seconds to erase everything in volitale memory.

![untz step sequencer](/../master/rotate1.gif?raw=true)

Rotate button 1 up and down to adjust note pitch range (notes are lost when they scroll off the top or bottom, can be used to quickly erase a single grid).

Press button 2 to select the layer, press button 2 again to select the grid set, press it again to return to normal. The value is indicated by the position of the horizontal or vertical line on the grid counting from the top or the left. The botton layer (layer 8) is the performance layer (see below). The currently playing grid set will not switch to a newly selected grid set until it has finished playing the current one.

Rotate button 2 up and down to adjust tempo up or down.

Press button 3 to select a midi instrument between 1-64, press button 3 again to select a midi instrument between 65-128, press it again to return to normal. The value is indicated by the location of the intersection of the displayed horizontal and vertical line. The value is changed by selecting a new value while the current value is being displayed.

Rotate button 3 to adjust note sequence measure view left or right. You can have up to 16 note beats per measure.

Press button 4 to select the measure size, press it again to return to normal. The value is indicated by the location of the intersection of the displayed horizontal and vertical line counting left to right and down. The value is changed by selecting a new value while the current value is being displayed.

Hold button 4 for ten seconds to store everything in volatile mememory into non-volitale memory.

Rotate button 4 up and down to adjust volume up or down.

Note: Depending on how you wire the buttons and encoders you may find these functions located on different buttons or encoders.

Note: The performance layer is available on each grid set as layer 8 and is not used for storage of note sequences. Selecting this layer will change the meaning of all the grid buttons on the device. The right most column of buttons display the currently playing grid set and can be used to select which grid set will play next. The rest of the buttons are layed out in rows of octive notes. The rows are indivudal octive scales and have the lowest key on the left and the highest key on the right based on the currently selected scale. Scales that have more or less then seven notes will not be so well organized. Each octive row are stacked so the highest octives are on top and the lowest are on the bottom. You can use this mode to play along with the sequences you have stored.
