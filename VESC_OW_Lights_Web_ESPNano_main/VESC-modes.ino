// headlights determined by colorpicker, tailights red
long Default(long mode_position) {
  return mode_position;                   //  Seq position not relevant for this so return current
}

// classic LED rainbow
long Rainbow(long mode_position) {
  return mode_position;                   //  Seq position not relevant for this so return current
}


// wipes new color across strip
long ColorWipe(long mode_position) {
  return mode_position;               // return 0 or 1
}

// fades whole strip smoothly between colors
long ColorFade(long mode_position) {
  return mode_position;
}


// red wavy fade
long Lava(long mode_position) {
  return mode_position;
}


// green wavy fade
long Canopy(long mode_position) {
  return mode_position;
}

// blue wavy fade
long Ocean(long mode_position) {
  return mode_position;
}


// wavy fade with rainbow colorshift
long RollingWave(long mode_position) {
  return mode_position +1;
}


// wavy fade of chosen color
long ColorWave(long mode_position) {
  return mode_position;
}

// flickering fireflies
long Fireflies(long mode_position) {
  return mode_position;
}


// multicolored confetti
long Confetti(long mode_position) {
  return mode_position;
}

// moving comet with tail
long Comet(long mode_position) {
  return mode_position;
}

// pacman eats more the faster you go!
long PacMan(long mode_position) {
  return mode_position;
}

// white headlights, custom color tail lights
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