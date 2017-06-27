#pragma once
/*
===========================================================================
This is the library for Beam.

Beam is a beautiful LED matrix — features 120 LEDs that displays scrolling text, animations, or custom lighting effects.
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
#define MAXFRAME 36
#define SPACE     3
#define KERNING   1


const uint8_t BEAM_ADDRESS[] = {0x36, 0x34, 0x30, 0x37};
#define BEAMA BEAM_ADDRESS[0]
#define BEAMB BEAM_ADDRESS[1]
#define BEAMC BEAM_ADDRESS[2]
#define BEAMD BEAM_ADDRESS[3]

//Sub Register address
enum BEAM_REGISTER {
  PIC       = 0x00,
  MOV       = 0x01,
  MOVMODE   = 0x02,
  FRAMETIME = 0x03,
  DISPLAYO  = 0x04,
  CURSRC    = 0x05,
  CFG       = 0x06,
  IRQMASK   = 0x07,
  IRQFRAME  = 0x08,
  SHDN      = 0x09,
  CLKSYNC   = 0x0B,
  //RAM section address
  CTRL      = 0xC0,
  REGSEL    = 0xFD,
};

//User modes
enum BEAM_MODE {
  PICTURE   = 0x01,
  MOVIE     = 0x02,
  SCROLL    = 0x03,
  FADEOFF   = 0x00,
  FADEON    = 0x01,
};

enum BEAM_ORIENTATION {
  RIGHT     = 0,
  LEFT      = 1,
};

class Beam {
public:
  Beam(int rstpin, int irqpin, int numberOfBeams);
  Beam(int rstpin, int irqpin, uint8_t syncMode, uint8_t beamAddress);
  bool begin(TwoWire& wire = Wire);
  void initBeam();
  void print(const char* text);
  void printFrame(uint8_t frameToPrint, const char * text);
  void play();
  void display();
  void draw();
  void setScroll(uint8_t direction, uint8_t fade);
  void setSpeed(uint8_t speed);
  void setLoops(uint8_t loops);
  void setMode(uint8_t mode);
  volatile int beamNumber;
  int checkStatus();
  int status();

private:
  const uint8_t *BEAM;
  uint16_t cs[12];
  uint16_t segmentmask[8];
  uint8_t  cscolumn[25];
  uint8_t  activeBeams;
  uint8_t  _gblMode;
  uint8_t  _syncMode;
  uint8_t  _lastFrameWrite;
  uint8_t  _scrollMode;
  uint8_t  _scrollDir;
  uint8_t  _fadeMode;
  uint8_t  _frameDelay;
  uint8_t  _beamMode;
  uint8_t  _numLoops;
  uint8_t  _beamCount;
  int      _rst;
  int      _irq;
  TwoWire *_wire;
  Timer   *_syncTimer;

  void startNextBeam();
  void initializeBeam(uint8_t b);
  void setPrintDefaults(uint8_t mode, uint8_t startFrame, uint8_t numFrames, uint8_t numLoops, uint8_t frameDelay, uint8_t scrollDir, uint8_t fadeMode);
  void writeFrame(uint8_t addr, uint8_t f);
  void convertFrame(const uint8_t * currentFrame);
  unsigned int setSyncTimer();
  void sendWriteCmd(uint8_t addr, uint8_t ramsection, uint8_t subreg, uint8_t subregdata);
  uint8_t sendReadCmd(uint8_t addr, uint8_t ramsection, uint8_t subreg);
  uint8_t i2cwrite(uint8_t address, uint8_t cmdbyte, uint8_t databyte);
};

