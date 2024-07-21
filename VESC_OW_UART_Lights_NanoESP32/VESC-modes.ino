
long Default(long mode_position) {
  return mode_position;                   //  Seq position not relevant for this so return current
}

// turns all LEDs on based on colors in colors array
// onoff option sets colors from right to left instead of left to right
long Rainbow(long mode_position) {
  return mode_position;                   //  Seq position not relevant for this so return current
}


// turns all LEDs on based on colors in colors array
// flashes alternate between on and off
// onoff option sets colors from right to left instead of left to right
long ColorWipe(long mode_position) {
  return mode_position;               // return 0 or 1
}

// Chaser - moves LEDs to left or right
// Only uses colours specified unless only one color in which case use second as black
// Colors in order passed to it (starting from first pixel)
// forward direction is moving away from first pixel
// onoff direction moves towards first pixel
long ColorFade(long mode_position) {
  return mode_position;
}


/* This is not currently used - same as chaserChangeColor if you only select one color */
// chaser using only a single color
// show 4 LEDs on, followed by 4 LEDs off
long Lava(long mode_position) {
  return mode_position;
}


// chaser using only a single color at a time
// show 4 LEDs on, followed by 4 LEDs off
// Note if the number of pixels is divisible by 8 then change on single block
// otherwise may change in a block of colors
long Canopy(long mode_position) {
  return mode_position;
}

// chaser with black background
// If multiple colours then a single block of colours goes across the Strip
// If single color selected then show 4 LEDs on, followed by 4 LEDs off
long Ocean(long mode_position) {
  return mode_position;
}


// Single LED chaser to end and then fill up
// If multiple colours then color fills up at end - led gets it's final color
long RollingWave(long mode_position) {
  return mode_position +1;
}


// From first pixel to last add LED at a time then stay lit
long ColorWave(long mode_position) {
  return mode_position;
}

// From all on remove LED at a time then stay off
long Fireflies(long mode_position) {
  return mode_position;
}


// turn all on one at a time, then all off again
long Confetti(long mode_position) {
  return mode_position;
}


long Comet(long mode_position) {
  return mode_position;
}

// From inside going outwards (both ends)
// Color applies equally from both ends
// goes from sequence 0 (none on) until 
//     sequence = (num_pixels/2)+1 if even
//     sequence = (num_pixels/2)+2 if odd 
long PacMan(long mode_position) {
  return mode_position;
}

long TailColor(long mode_position) {
  return mode_position;
}


// From outside going inwards (both ends) turning off
// Color applies equally from both ends
// goes from sequence 0 (all on) until 
//     sequence = (num_pixels/2)+1 if even
//     sequence = (num_pixels/2)+2 if odd 
long PixelFinder(long mode_position) {
  return mode_position;
}


// From inwards going outwards (both ends) turning off
// Color applies equally from both ends
// goes from sequence 0 (all on) until 
//     sequence = (num_pixels/2)+1 if even
//     sequence = (num_pixels/2)+2 if odd 
long unused0(long mode_position) {
  return mode_position;
}



// colorWipeInOut
//Turn on in sequence going inwards, then out again. Starting at both ends.
long unused1(long mode_position) {
  return mode_position;
}


// colorWipeOutIn
//Turn on in sequence going outwards, then in again. Starting at both ends.
long unused2(long mode_position) {
  return mode_position;
}