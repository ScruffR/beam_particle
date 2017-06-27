/*
===========================================================================
This is the library for Beam.

Beam is a beautiful LED matrix — features 120 LEDs that displays scrolling 
text, animations, or custom lighting effects.
Beam can be purchased here: http://www.hoverlabs.co

Written by Emran Mahbub and Jonathan Li for Hover Labs.
BSD license, all text above must be included in any redistribution

---------------------------------------------------------------------------

Edit by ScruffR 2017-06-25:
Adapted to support alternative I2C interfaces
Added support for special characters (e.g. German Umlauts Ä, Ö, Ü) with the
  framework to add any other
Replaced "copy-paste" logic with functional blocks by use of arrays and loops

===========================================================================
*/
#include <Particle.h>
#include "beam.h"
#include "charactermap.h"
#include "frames.h"

/*
=================
PUBLIC FUNCTIONS
=================
*/

/*
This constructor used when multiple Beams behave like one long Beam
*/
Beam::Beam(int rstpin, int irqpin, int numberOfBeams) {
  Log.trace("Beam::Beam(int rstpin, int irqpin, int numberOfBeams)");
  _rst = rstpin;
  _irq = irqpin;

  if (numberOfBeams <= 0 || 4 <= numberOfBeams) {
    Log.warn("Number of Beams must be between 1 and 4 and not %d (default to 1 BEAMA)", numberOfBeams);
    numberOfBeams = 1;
  }
  BEAM = &BEAM_ADDRESS[0];
  activeBeams = 
  _beamCount = numberOfBeams;
  _gblMode = 1;
}

/*
This constructor used when multiple Beams behave like single Beam units
*/
Beam::Beam(int rstpin, int irqpin, uint8_t syncMode, uint8_t beamAddress) {
  Log.trace("Beam::Beam(int rstpin, int irqpin, uint8_t syncMode, uint8_t beamAddress)");
  _rst = rstpin;
  _irq = irqpin;
  _syncMode = 0;
  _beamCount = 
  activeBeams = 1;
  BEAM = NULL;
  for (unsigned int b = 0; b < sizeof(BEAM_ADDRESS); b++) {
    if (BEAM_ADDRESS[b] == beamAddress) {
      BEAM = &BEAM_ADDRESS[b];
      break;
    }
  }
  if (!BEAM) {
    BEAM = &BEAM_ADDRESS[0];
    Log.warn("%02x is not a valid Beam address (default to BEAMA %02x)", beamAddress, BEAM_ADDRESS[0]);
  }

  _gblMode = 0;
}

bool Beam::begin(TwoWire& wire) {
  Log.trace("bool Beam::begin(TwoWire& wire)");
  _wire = &wire;

  //resets beam - will clear all beams
  pinMode(_rst, OUTPUT);
  digitalWrite(_rst, LOW);
  delay(100);
  digitalWrite(_rst, HIGH);
  delay(250);

  //reset cs[]
  memset((uint8_t*)cs, 0x00, sizeof(cs));

  //reset segmentmask[]
  for (int s = 0; s < 8; s++) {
    segmentmask[s] = 0x0001 << (7 - s);
  }

  return true;
}

void Beam::initBeam() {
  Log.trace("void Beam::initBeam()");
  //initialize Beam 
  for (unsigned int b = 0; b < _beamCount; b++) {
    Log.trace("clearing BEAM[%d]", b);
    initializeBeam(BEAM[b]);
  }
}

void Beam::print(const char* text) {
  Log.trace("void Beam::print(const char* text)");
  //resets beam - will clear all beams
  pinMode(_rst, OUTPUT);
  digitalWrite(_rst, LOW);
  delay(100);
  digitalWrite(_rst, HIGH);
  delay(250);

  Log.info("Text to print: %s", text);

  initBeam();

  // Clear all frames
  memset((uint8_t*)cs, 0x00, sizeof(cs));

  for (int i = 0; i < 36; i++) {
    for (unsigned int b = 0; b < _beamCount; b++) {
      writeFrame(BEAM[b], i);
    }
  }

  int i = 0;
  const uint8_t *fontptr;
  int frame = 0;

  int asciiVal;
  int cscount = 0;
  int stringLen = strlen(text);

  while ((i < stringLen) && frame < 36) {
    // pick a character to print to Beam
    asciiVal = toupper(text[i]);
    if (32 <= asciiVal && asciiVal <= 96) {
      fontptr = &charactermap[(asciiVal - 32)][0];   //set fontptr to matching font
    }
    else {
      switch (text[i]) {
        case 0xC3: // two byte character prefix to be ignored
          i++;
          continue;
          break;
        case 0x84: // (0xC3 0x84) 'Ä'
        case 0xA4: // (0xC3 0xA4) 'ä'
          fontptr = &charactermap[65][0];
          break;
        case 0x96: // (0xC3 0x96) 'Ö'
        case 0xB6: // (0xC3 0x86) 'ö'
          fontptr = &charactermap[66][0];
          break;
        case 0x9C: // (0xC3 0x9C) 'Ü'
        case 0xBC: // (0xC3 0xBC) 'ü'
          fontptr = &charactermap[67][0];
          break;
        case 0x9F: // (0xC3 0xDF) 'ß'
          fontptr = &charactermap[68][0];
          break;
        default:
          fontptr = &charactermap[0][0];
          break;
      }
    }

    // loop through the Beam grid and place characters
    // from the character map
    Log.trace("%c = %d (0x%02x)\r\ncscolumn[] = ", text[i], asciiVal, asciiVal);
    while (cscount < 24 && *fontptr != 0xFF) {
      cscolumn[cscount] = *fontptr;
      Log.trace("0x%02x", cscolumn[cscount]);
      fontptr++;
      cscount++;
    }
    i++;  // go to next character

    if (cscount > 23) {
      // if end of grid is reached in current frame,
      // then start writing to the Beam registers
      Log.trace("--- end of frame reached ---\r\nwriting cs[]");
      for (int j = 0; j < 12; j++) {
        cs[j] = (cscolumn[j * 2]) | (cscolumn[j * 2 + 1] << 5);
        Log.trace("0x%02x", cs[j]);
      }
      Log.trace("--- end of cs[] ---");

      for (unsigned int b = 0; b < _beamCount; b++) {
        writeFrame(BEAM[b], frame + (_beamCount - b));
      }
      _lastFrameWrite = frame + _beamCount;

      for (int x = 0; x < 12; x++) {
        cs[x] = 0x00;
        cscolumn[x * 2] = 0x00;
        cscolumn[x * 2 + 1] = 0x00;
      }
      frame++;            // go to next frame
      cscount = 0;        // reset cscount

      // if a specific frame is specified, then return if that frame is done.
      //ADD THIS LATER
      //if (frameNum!=0 && frame > frameNum){
      //    return;
      //}

      if (*fontptr != 0xFF) {
        //special case if current character needs to wrap to next frame
        Log.trace("Continuing prev char cscolumn[] = ");

        while (cscount < 24 && *fontptr != 0xFF) {
          cscolumn[cscount] = *fontptr;
          Log.trace("0x%02x", cscolumn[cscount]);
          fontptr++;
          cscount++;
        }
      }
    }

    if (stringLen == i) {
      // if end of string is reached in current frame,
      // then start writing to Beam registers
      Log.trace("--- end of string reached ---\r\nwriting cs[]");
      for (int j = 0; j < 12; j++) {
        cs[j] = (cscolumn[j * 2]) | (cscolumn[j * 2 + 1] << 5);
        Log.trace("0x%02x", cs[j]);
      }
      Log.trace("--- done cs print ---");

      for (unsigned int b = 0; b < _beamCount; b++) {
        writeFrame(BEAM[b], frame + (_beamCount - b));
      }
      _lastFrameWrite = frame + _beamCount;

      for (int x = 0; x < 12; x++) {
        cs[x] = 0x00;
        cscolumn[x * 2] = 0x00;
        cscolumn[x * 2 + 1] = 0x00;
      }
    }
  }

  //defaults Beam to basic settings
  setPrintDefaults(SCROLL, 0, 6, 7, 5, 1, 0);
}

void Beam::printFrame(uint8_t frameToPrint, const char * text) {
  Log.trace("void Beam::printFrame(uint8_t frameToPrint, const char * text)");
  Log.info("Text to print: %s", text);

  int i = 0;
  const uint8_t * fontptr;
  int frame = frameToPrint;

  int asciiVal;
  int cscount = 0;
  int stringLen = strlen(text);

  while ((i < stringLen) && frame < 36) {
    // pick a character to print to Beam
    asciiVal = toupper(text[i]);
    if (32 <= asciiVal && asciiVal <= 96) {
      fontptr = &charactermap[(asciiVal - 32)][0];   //set fontptr to matching font
    }
    else {
      switch (text[i]) {
        case 0xC3: // two byte character prefix to be ignored
          i++;
          continue;
          break;
        case 0x84: // (0xC3 0x84) 'Ä'
        case 0xA4: // (0xC3 0xA4) 'ä'
          fontptr = &charactermap[65][0];
          break;
        case 0x96: // (0xC3 0x96) 'Ö'
        case 0xB6: // (0xC3 0x86) 'ö'
          fontptr = &charactermap[66][0];
          break;
        case 0x9C: // (0xC3 0x9C) 'Ü'
        case 0xBC: // (0xC3 0xBC) 'ü'
          fontptr = &charactermap[67][0];
          break;
        case 0x9F: // (0xC3 0xDF) 'ß'
          fontptr = &charactermap[68][0];
          break;
        default:
          fontptr = &charactermap[0][0];
          break;
      }
    }

    Log.trace("%c = %d (0x%02x)\r\ncscolumn[] = ", text[i], asciiVal, asciiVal);
    // loop through the Beam grid and place characters
    // from the character map
    while (cscount < 24 && *fontptr != 0xFF) {
      cscolumn[cscount] = *fontptr;
      Log.trace("0x%02x", cscolumn[cscount]);
      fontptr++;
      cscount++;
    }
    i++;  // go to next character

    if (cscount > 23) {
      // if end of grid is reached in current frame,
      // then start writing to the Beam registers

      Log.trace("--- end of frame reached ---\r\nwriting cs[]");
      for (int j = 0; j < 12; j++) {
        cs[j] = (cscolumn[j * 2]) | (cscolumn[j * 2 + 1] << 5);
        Log.trace("0x%02x", cs[j]);
      }
      Log.trace("--- end of cs[] ---");

      for (unsigned int b = 0; b < _beamCount; b++) {
        writeFrame(BEAM[b], frame);
        //_lastFrameWrite = frame;
      }

      for (int x = 0; x < 12; x++) {
        cs[x] = 0x00;
        cscolumn[x * 2] = 0x00;
        cscolumn[x * 2 + 1] = 0x00;
      }

      frame++;            // go to next frame
      cscount = 0;        // reset cscount
      _lastFrameWrite = frame;

      // if a specific frame is specified, then return if that frame is done.
      if (frameToPrint != 0 && frame > frameToPrint) {
        //defaults Beam to basic settings
        setPrintDefaults(SCROLL, 0, _lastFrameWrite, 7, 15, 1, 1);
        return;
      }
    }
  }
}

void Beam::play() {
  Log.trace("void Beam::play()");
  //start playing beams depending on scroll direction
  if (_scrollDir == LEFT) {
    sendWriteCmd(BEAM[_beamCount - 1], CTRL, SHDN, 0x03);
  }
  else {
    sendWriteCmd(BEAM[0], CTRL, SHDN, 0x03);
  }

  if (_beamCount > 1) {
    while (checkStatus() != 1) {
      delay(10);
    }
  }
}

void Beam::startNextBeam() {
  Log.trace("void Beam::startNextBeam()");
  Log.trace("_scrollDir: %d, _beamCount: %d, beamNumber: %d", _scrollDir, _beamCount, beamNumber);

  // since the original logic didn't make much sense, I assumed similar behaviour to Beam::play() might make more sense
  // see https://github.com/hoverlabs/beam_particle/issues/4
  //start playing beams depending on scroll direction
  if (_scrollDir == LEFT) {
    sendWriteCmd(BEAM[_beamCount - 1], CTRL, SHDN, 0x03);
  }
  else {
    sendWriteCmd(BEAM[0], CTRL, SHDN, 0x03);
  }
}

void Beam::setScroll(uint8_t direction, uint8_t fade) {
  Log.trace("void Beam::setScroll(uint8_t direction, uint8_t fade)");
  if (direction != RIGHT && direction != LEFT) {
    Log.warn("Select either LEFT or RIGHT for direction");
    return;
  }

  _scrollDir = direction;
  _fadeMode = fade;
  _scrollMode = 1;

  uint8_t frameData = _fadeMode << 7 | _scrollDir << 6 | 0 << 5 | _scrollMode << 4 | _frameDelay;

  for (unsigned int b = 0; b < _beamCount; b++) {
    sendWriteCmd(BEAM[b], CTRL, FRAMETIME, frameData);
  }
}

void Beam::setSpeed(uint8_t speed) {
  Log.trace("void Beam::setSpeed(uint8_t speed)");
  if (speed < 1 || 15 < speed) {
    Log.trace("Enter a speed between 1 and 15");
    return;
  }

  _scrollMode = (_beamMode == MOVIE) ? 0 : 1;

  _frameDelay = speed;

  uint8_t frameData = _fadeMode << 7 | _scrollDir << 6 | 0 << 5 | _scrollMode << 4 | _frameDelay;

  for (unsigned int b = 0; b < _beamCount; b++) {
    sendWriteCmd(BEAM[b], CTRL, FRAMETIME, frameData);
  }
}

void Beam::setLoops(uint8_t loops) {
  Log.trace("void Beam::setLoops(uint8_t loops)");
  if (loops < 1 || 7 < loops) {
    Log.warn("Enter a speed between 1 and 7");
    return;
  }

  _numLoops = loops;
  uint8_t displayData = _numLoops << 5 | 0 << 4 | 0x0B;

  for (unsigned int b = 0; b < _beamCount; b++) {
    sendWriteCmd(BEAM[b], CTRL, DISPLAYO, displayData);
  }
}

void Beam::setMode(uint8_t mode) {
  Log.trace("void Beam::setMode(uint8_t mode)");
  if (mode != MOVIE && mode != SCROLL) {
    Log.warn("Select either SCROLL or MOVIE for mode");
    return;
  }

  _beamMode = mode;
  uint8_t frameData = 0;

  if (mode == MOVIE) {
    frameData = 0 << 7 | 0 << 6 | 0 << 5 | 0 << 4 | _frameDelay;
  }
  else if (mode == SCROLL) {
    frameData = _fadeMode << 7 | _scrollDir << 6 | 0 << 5 | _scrollMode << 4 | _frameDelay;
  }

  for (unsigned int b = 0; b < _beamCount; b++) {
    sendWriteCmd(BEAM[b], CTRL, FRAMETIME, frameData);
  }
}

/*
Used by global mode to check when daisy chained Beams
should be activated depending on the scroll direction.
*/
int Beam::checkStatus() {
  Log.trace("int Beam::checkStatus()");
  if ((sendReadCmd(BEAM[activeBeams - 1], CTRL, 0x0F) >> 2) == (_beamCount - activeBeams + 1)) {
    sendWriteCmd(BEAM[--activeBeams - 1], CTRL, SHDN, 0x03);
    if (activeBeams <= 1) {
      delay(10);
      activeBeams = _beamCount;
      return 1;
    }
  }
  
  return 0;
}

void Beam::draw() {
  Log.trace("void Beam::draw()");
  //resets beam - will clear all beams
  pinMode(_rst, OUTPUT);
  digitalWrite(_rst, LOW);
  delay(100);
  digitalWrite(_rst, HIGH);
  delay(250);

  initBeam();

  for (int i = 0; i < 36; ++i) {
    convertFrame(frameList[i]);
    // altered original frame counting logic: see https://github.com/hoverlabs/beam_particle/issues/6
    for (unsigned int b = 0; b < _beamCount; b++) {
      writeFrame(BEAM[b], i + (_beamCount - 1 - b));
    }
    _lastFrameWrite = i + _beamCount - 1;

    // reset cs[]
    memset((uint8_t*)cs, 0x00, sizeof(cs));
  }

  setPrintDefaults(MOVIE, 1, 20, 7, 2, 1, 0);
}

void Beam::display() {
  uint8_t pictureData = 0 << 7 | 1 << 6 | _beamCount;
  uint8_t displayData = 0 << 7 | 0 << 6 | 0 << 5 | 0 << 4 | 0x0B;
  uint8_t currsrcData = 0;

  // change led current based on number of connected beams
  switch (_beamCount) {
    case 1:
      currsrcData = 0x20;
      break;
    case 2:
      // unexpected value: see https://github.com/hoverlabs/beam_particle/issues/5
      currsrcData = 0x15;
      break;
    case 3:
      currsrcData = 0x10;
      break;
    case 4:
      currsrcData = 0x08;
      break;
    default:
      currsrcData = 0x00;
      break;
  }

  for (unsigned int b = 0; b < _beamCount; b++) {
    sendWriteCmd(BEAM[b], CTRL, PIC, pictureData);
    sendWriteCmd(BEAM[b], CTRL, CURSRC, currsrcData);
    sendWriteCmd(BEAM[b], CTRL, DISPLAYO, displayData);
  }

  for (unsigned int b = 0; b < _beamCount; b++) {
    sendWriteCmd(BEAM[b], CTRL, SHDN, 0x03);
  }
}

int Beam::status() {
  int frameDone = 0;

  if (_gblMode == 0) {
    frameDone = (sendReadCmd(BEAM[0], CTRL, 0x0F) >> 2);
    Log.trace("Frame done (%d)", frameDone);
  }
  return frameDone;
}

/*
=================
PRIVATE FUNCTIONS
=================
*/

void Beam::initializeBeam(uint8_t baddr) {
  Log.trace("void Beam::initializeBeam(uint8_t baddr)");
  //set basic config on each defined beam unit
  sendWriteCmd(baddr, CTRL, CFG, 0x01);

  //set each frame to off since cs[] is reset by default 
  for (int i = 0; i < 36; i++) {
    writeFrame(baddr, i);
  }

  //set basic blink + pwm registers for each defined beam
  for (int i = 0x40; i <= 0x45; i++) {
    for (int j = 0x00; j <= 0x17; j++) {
      sendWriteCmd(baddr, i, j, 0x00);
    }
    for (int k = 0x18; k <= 0x9b; k++) {
      sendWriteCmd(baddr, i, k, 0xFF);
    }
  }
}

void Beam::setPrintDefaults(uint8_t mode, uint8_t startFrame, uint8_t numFrames, uint8_t numLoops, uint8_t frameDelay, uint8_t scrollDir, uint8_t fadeMode) {
  Log.trace("void Beam::setPrintDefaults(uint8_t mode, uint8_t startFrame, uint8_t numFrames, uint8_t numLoops, uint8_t frameDelay, uint8_t scrollDir, uint8_t fadeMode)");
  _scrollMode = 1;
  _scrollDir = scrollDir;
  _fadeMode = fadeMode;
  _frameDelay = frameDelay;
  _beamMode = mode;
  _numLoops = numLoops;

  if (mode == MOVIE || mode == SCROLL) {
    //make sure startFrame between 0 and 35
    //make sure numFrames between 2 and 36
    //make sure frameDelay between 0 and 1111
    //make sure numLoops between 000 and 111

    uint8_t movieData = 0 << 7 | 1 << 6 | startFrame;
    uint8_t moviemodeData = 0 << 7 | 0 << 6 | _lastFrameWrite;
    uint8_t frameData = 0;
    //uint8_t syncData = 0;

    switch (mode) {
      case MOVIE:
        frameData = 0 << 7 | 0 << 6 | 0 << 5 | 0 << 4 | frameDelay;
        break;
      case SCROLL:
        frameData = fadeMode << 7 | scrollDir << 6 | 0 << 5 | _scrollMode << 4 | frameDelay;
        break;
    }

    uint8_t displayData = _numLoops << 5 | 0 << 4 | 0x0B;
    //uint8_t irqmaskData = 0xFF;
    //uint8_t irqframedefData = 0x03;
    uint8_t currsrcData = 0;

    // change led current based on number of connected beams
    if (_beamCount == 4) {
      currsrcData = 0x08;
    }
    else if (_beamCount == 3) {
      currsrcData = 0x10;
    }
    else if (_beamCount == 2) {
      currsrcData = 0x20;
    }
    else if (_beamCount == 1) {
      currsrcData = 0x20;
    }
    else {
      currsrcData = 0x15;
    }

    if (_scrollDir == LEFT) {
      for (unsigned int b = 0; b < _beamCount; b++) {
          
        sendWriteCmd(BEAM[b], CTRL, MOV, movieData);
        sendWriteCmd(BEAM[b], CTRL, MOVMODE, moviemodeData);
        sendWriteCmd(BEAM[b], CTRL, CURSRC, currsrcData);
        sendWriteCmd(BEAM[b], CTRL, FRAMETIME, frameData);
        sendWriteCmd(BEAM[b], CTRL, DISPLAYO, displayData);
        if (b != 3)  // for some reason not for BEAMD (???)
          sendWriteCmd(BEAM[b], CTRL, SHDN, 0x02);
      }
    }
    else {
      //NEED TO MODIFY  FOR RIGHT OR LEFT SCROLL//
    }

    if (_gblMode == 1 && _beamCount > 1) {
      /* define clk sync in/out settings based on left/right scrolling direction */
      if (_scrollDir == LEFT) {
        sendWriteCmd(BEAM[_beamCount - 1], CTRL, CLKSYNC, 0x02);
        for (int b = 0; b < _beamCount - 1; b++) {
          sendWriteCmd(BEAM[b], CTRL, CLKSYNC, 0x01);
        }
      }
      else {
        sendWriteCmd(BEAM[0], CTRL, CLKSYNC, 0x02);
        for (unsigned int b = 1; b < _beamCount; b++) {
          sendWriteCmd(BEAM[b], CTRL, CLKSYNC, 0x01);
        }
      }
    }
    else {
      // ToDo
    }
  }
}

unsigned int Beam::setSyncTimer() {
  Log.trace("unsigned int Beam::setSyncTimer()");
  if (1 <= _frameDelay && _frameDelay <= 15)
    return _frameDelay * 32.5;

  return 1000;
}

void Beam::writeFrame(uint8_t addr, uint8_t f) {
  Log.trace("void Beam::writeFrame(uint8_t addr, uint8_t f)");
  uint8_t p = f;
  Log.trace("writing frame %c (0x%02x)", p, p);
  int data = 0;
  for (int j = 0x00; j <= 0x0B; j++) {
    sendWriteCmd(addr, p + 1, 2 * j, cs[data] & 0xFF);          // i = frame address, 2*j = frame register address (even numbers) then first data byte
    sendWriteCmd(addr, p + 1, 2 * j + 1, (cs[data] & 0x300) >> 8);    // i = frame address, 2*j+1 = frame register address (odd numbers) then second data byte
    data++;
  }
  Log.trace("Done writing frame");
}

void Beam::convertFrame(const uint8_t * currentFrame) {
  Log.trace("void Beam::convertFrame(const uint8_t * currentFrame)");
  int i = 0;

  //CS0 to CS3
  int n = 0;
  for (int y = 10; y > 0; --y) {
    i = (y < 6) ? 1 : 0;
    cs[0] |= (((uint16_t)(*(currentFrame + n) & segmentmask[0 + i]) << (3 + i)) >> y);
    cs[1] |= (((uint16_t)(*(currentFrame + n) & segmentmask[2 + i]) << (5 + i)) >> y);
    cs[2] |= (((uint16_t)(*(currentFrame + n) & segmentmask[4 + i]) << (7 + i)) >> y);
    cs[3] |= (((uint16_t)(*(currentFrame + n) & segmentmask[6 + i]) << (9 + i)) >> y);
    n += 3;

    if (n > 12) {
      n = 0;
    }
  }

  //CS4 to CS7
  n = 1;
  for (int y = 10; y > 0; --y) {
    i = (y < 6) ? 1 : 0;
    cs[4] |= (((uint16_t)(*(currentFrame + n) & segmentmask[0 + i]) << (3 + i)) >> y);
    cs[5] |= (((uint16_t)(*(currentFrame + n) & segmentmask[2 + i]) << (5 + i)) >> y);
    cs[6] |= (((uint16_t)(*(currentFrame + n) & segmentmask[4 + i]) << (7 + i)) >> y);
    cs[7] |= (((uint16_t)(*(currentFrame + n) & segmentmask[6 + i]) << (9 + i)) >> y);
    n += 3;

    if (n > 13) {
      n = 1;
    }
  }

  //CS8 - CS11
  n = 2;
  for (int y = 10; y > 0; --y) {
    i = (y < 6) ? 1 : 0;
    cs[8] |= (((uint16_t)(*(currentFrame + n) & segmentmask[0 + i]) << (3 + i)) >> y);
    cs[9] |= (((uint16_t)(*(currentFrame + n) & segmentmask[2 + i]) << (5 + i)) >> y);
    cs[10] |= (((uint16_t)(*(currentFrame + n) & segmentmask[4 + i]) << (7 + i)) >> y);
    cs[11] |= (((uint16_t)(*(currentFrame + n) & segmentmask[6 + i]) << (9 + i)) >> y);
    n += 3;

    if (n > 14) {
      n = 2;
    }
  }
}

void Beam::sendWriteCmd(uint8_t addr, uint8_t ramsection, uint8_t subreg, uint8_t subregdata) {
  //Log.trace("void Beam::sendWriteCmd(uint8_t addr, uint8_t ramsection, uint8_t subreg, uint8_t subregdata)");
  static int errCount = 0;
  if (!i2cwrite(addr, REGSEL, ramsection)) {
    i2cwrite(addr, subreg, subregdata);
    errCount = 0;
  }
  else {
    Log.warn("Beam not found: 0x%02x (%d)", addr, _beamCount);
    if (errCount++ > 50) _wire->reset();
  }
}

uint8_t Beam::sendReadCmd(uint8_t addr, uint8_t ramsection, uint8_t subreg) {
  //Log.trace("uint8_t Beam::sendReadCmd(uint8_t addr, uint8_t ramsection, uint8_t subreg)");
  i2cwrite(addr, REGSEL, ramsection);

  _wire->beginTransmission(addr);
  _wire->write(subreg);
  _wire->endTransmission();

  _wire->requestFrom(addr, (uint8_t)1);
    // wait up to 250ms for data  
  for (uint32_t _ms = millis(); !_wire->available() && millis() - _ms < 250; Particle.process());
  if (_wire->available()) return _wire->read();
  else _wire->reset();
  return 0;
}

uint8_t Beam::i2cwrite(uint8_t address, uint8_t cmdbyte, uint8_t databyte) { 
  //Log.trace("uint8_t Beam::i2cwrite(uint8_t address, uint8_t cmdbyte, uint8_t databyte)");
  _wire->beginTransmission(address);
  _wire->write(cmdbyte);
  _wire->write(databyte);
  return (_wire->endTransmission());
}
