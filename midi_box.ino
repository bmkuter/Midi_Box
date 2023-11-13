#include <MIDI.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

// Rotary Encoder Inputs
#define CLK 34
#define DT 25
#define SW 39

enum SelectMode { SELECT_ROOT, SELECT_SCALE, ADJUST_BPM, RUNNING_MODE }; // Enum to define selection modes
SelectMode selectMode = RUNNING_MODE; // Start with selecting root note

int16_t inputDelta = 0;             // Counts up or down depending which way the encoder is turned
int16_t old_inputDelta = 0;
bool printFlag = false;             // Flag to indicate that the value of inputDelta should be printed

bool buttonPressed = false;
bool lastButtonState = HIGH;  // Assuming the button is active LOW
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
volatile long lastEncoderPosition = 0;
enum Mode { ROOT_NOTE,
            SCALE };
Mode currentMode = ROOT_NOTE;
const int encoderSensitivity = 1;  // Adjust this value as needed
int encoderPulseCount = 0;
int indexRoot = 0;
int indexScale = 0;

// Global MIDI clock control structure
struct MidiClockControl {
  volatile bool running;
  volatile int bpm;
};

MidiClockControl midiClockControl = {false, 120};  // Default: stopped, 60 BPM

ESP32Encoder encoder;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES 10  // Number of snowflakes in the animation example

#define LOGO_HEIGHT 16
#define LOGO_WIDTH 16
static const unsigned char PROGMEM logo_bmp[] = { 0b00000000, 0b11000000,
                                                  0b00000001, 0b11000000,
                                                  0b00000001, 0b11000000,
                                                  0b00000011, 0b11100000,
                                                  0b11110011, 0b11100000,
                                                  0b11111110, 0b11111000,
                                                  0b01111110, 0b11111111,
                                                  0b00110011, 0b10011111,
                                                  0b00011111, 0b11111100,
                                                  0b00001101, 0b01110000,
                                                  0b00011011, 0b10100000,
                                                  0b00111111, 0b11100000,
                                                  0b00111111, 0b11110000,
                                                  0b01111100, 0b11110000,
                                                  0b01110000, 0b01110000,
                                                  0b00000000, 0b00110000 };

#define XPOS 0  // Indexes into the 'icons' array in function below
#define YPOS 1
#define DELTAY 2

struct MidiBeat {
  int pitch;     // MIDI pitch value
  int velocity;  // MIDI velocity value
  int channel;   // MIDI channel
};

MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);

// Create a new MCP4725 object with the I2C address. If left blank, the default is 0x62.
Adafruit_MCP4725 dac;
const int gatePin = 26;  // Change to the pin you are using for gate output
uint32_t global_gate_tracker = 0;

enum ScaleType {
  C_MINOR,
  C_SHARP_MINOR,
  D_MINOR,
  D_SHARP_MINOR,
  E_MINOR,
  F_MINOR,
  F_SHARP_MINOR,
  C_HARMONIC_MINOR,
  C_SHARP_HARMONIC_MINOR,
  D_HARMONIC_MINOR,
  D_SHARP_HARMONIC_MINOR,
  E_HARMONIC_MINOR,
  F_HARMONIC_MINOR,
  F_SHARP_HARMONIC_MINOR,
  C_MAJOR,
  C_SHARP_MAJOR,
  D_MAJOR,
  D_SHARP_MAJOR,
  E_MAJOR,
  F_MAJOR,
  F_SHARP_MAJOR,
  C_HARMONIC_MAJOR,
  C_SHARP_HARMONIC_MAJOR,
  D_HARMONIC_MAJOR,
  D_SHARP_HARMONIC_MAJOR,
  E_HARMONIC_MAJOR,
  F_HARMONIC_MAJOR,
  F_SHARP_HARMONIC_MAJOR,
  G_MINOR,
  G_SHARP_MINOR,
  A_MINOR,
  A_SHARP_MINOR,
  B_MINOR,
  G_MAJOR,
  G_SHARP_MAJOR,
  A_MAJOR,
  A_SHARP_MAJOR,
  B_MAJOR,
  G_HARMONIC_MINOR,
  G_SHARP_HARMONIC_MINOR,
  A_HARMONIC_MINOR,
  A_SHARP_HARMONIC_MINOR,
  B_HARMONIC_MINOR,
  G_HARMONIC_MAJOR,
  G_SHARP_HARMONIC_MAJOR,
  A_HARMONIC_MAJOR,
  A_SHARP_HARMONIC_MAJOR,
  B_HARMONIC_MAJOR,
  C_LYDIAN,
  C_SHARP_LYDIAN,
  D_LYDIAN,
  D_SHARP_LYDIAN,
  E_LYDIAN,
  F_LYDIAN,
  F_SHARP_LYDIAN,
  G_LYDIAN,
  G_SHARP_LYDIAN,
  A_LYDIAN,
  A_SHARP_LYDIAN,
  B_LYDIAN,
  C_DORIAN,
  C_SHARP_DORIAN,
  D_DORIAN,
  D_SHARP_DORIAN,
  E_DORIAN,
  F_DORIAN,
  F_SHARP_DORIAN,
  G_DORIAN,
  G_SHARP_DORIAN,
  A_DORIAN,
  A_SHARP_DORIAN,
  B_DORIAN,
  C_LOCRIAN,
  C_SHARP_LOCRIAN,
  D_LOCRIAN,
  D_SHARP_LOCRIAN,
  E_LOCRIAN,
  F_LOCRIAN,
  F_SHARP_LOCRIAN,
  G_LOCRIAN,
  G_SHARP_LOCRIAN,
  A_LOCRIAN,
  A_SHARP_LOCRIAN,
  B_LOCRIAN,
  // The last scale type to signify the size of the enum
  NUMBER_OF_SCALES
};

ScaleType currentScale = C_MAJOR;  // Set this to the default or desired scale.

int scales[NUMBER_OF_SCALES][7] = {
  { 0, 2, 3, 5, 7, 8, 10 },   // C_MINOR
  { 1, 3, 4, 6, 8, 9, 11 },   // C_SHARP_MINOR
  { 0, 2, 4, 5, 7, 9, 10 },   // D_MINOR
  { 1, 3, 5, 6, 8, 10, 11 },  // D_SHARP_MINOR
  { 0, 2, 4, 6, 7, 9, 11 },   // E_MINOR
  { 0, 1, 3, 5, 7, 8, 10 },   // F_MINOR
  { 1, 2, 4, 6, 8, 9, 11 },   // F_SHARP_MINOR
  { 0, 2, 3, 5, 7, 8, 11 },   // C_HARMONIC_MINOR
  { 0, 1, 3, 4, 6, 8, 9 },    // C_SHARP_HARMONIC_MINOR
  { 1, 2, 4, 5, 7, 9, 10 },   // D_HARMONIC_MINOR
  { 2, 3, 5, 6, 8, 10, 11 },  // D_SHARP_HARMONIC_MINOR
  { 0, 3, 4, 6, 7, 9, 11 },   // E_HARMONIC_MINOR
  { 0, 1, 4, 5, 7, 8, 10 },   // F_HARMONIC_MINOR
  { 1, 2, 5, 6, 8, 9, 11 },   // F_SHARP_HARMONIC_MINOR
  { 0, 2, 4, 5, 7, 9, 11 },   // C_MAJOR
  { 0, 1, 3, 5, 6, 8, 10 },   // C_SHARP_MAJOR
  { 1, 2, 4, 6, 7, 9, 11 },   // D_MAJOR
  { 0, 2, 3, 5, 7, 8, 10 },   // D_SHARP_MAJOR
  { 1, 3, 4, 6, 8, 9, 11 },   // E_MAJOR
  { 0, 2, 4, 5, 7, 9, 10 },   // F_MAJOR
  { 1, 3, 5, 6, 8, 10, 11 },  // F_SHARP_MAJOR
  { 0, 2, 4, 5, 7, 8, 11 },   // C_HARMONIC_MAJOR
  { 0, 1, 3, 5, 6, 8, 9 },    // C_SHARP_HARMONIC_MAJOR
  { 1, 2, 4, 6, 7, 9, 10 },   // D_HARMONIC_MAJOR
  { 2, 3, 5, 7, 8, 10, 11 },  // D_SHARP_HARMONIC_MAJOR
  { 0, 3, 4, 6, 8, 9, 11 },   // E_HARMONIC_MAJOR
  { 0, 1, 4, 5, 7, 9, 10 },   // F_HARMONIC_MAJOR
  { 1, 2, 5, 6, 8, 10, 11 },  // F_SHARP_HARMONIC_MAJOR
  { 0, 2, 3, 5, 7, 9, 10 },   // G_MINOR
  { 1, 3, 4, 6, 8, 10, 11 },  // G_SHARP_MINOR
  { 0, 2, 4, 5, 7, 9, 11 },   // A_MINOR
  { 0, 1, 3, 5, 6, 8, 10 },   // A_SHARP_MINOR
  { 1, 2, 4, 6, 7, 9, 11 },   // B_MINOR
  { 0, 2, 4, 6, 7, 9, 11 },   // G_MAJOR
  { 0, 1, 3, 5, 7, 8, 10 },   // G_SHARP_MAJOR
  { 1, 2, 4, 6, 8, 9, 11 },   // A_MAJOR
  { 0, 2, 3, 5, 7, 9, 10 },   // A_SHARP_MAJOR
  { 1, 3, 4, 6, 8, 10, 11 },  // B_MAJOR
  { 0, 2, 3, 6, 7, 9, 10 },   // G_HARMONIC_MINOR
  { 1, 3, 4, 7, 8, 10, 11 },  // G_SHARP_HARMONIC_MINOR
  { 0, 2, 4, 5, 8, 9, 11 },   // A_HARMONIC_MINOR
  { 0, 1, 3, 5, 6, 9, 10 },   // A_SHARP_HARMONIC_MINOR
  { 1, 2, 4, 6, 7, 10, 11 },  // B_HARMONIC_MINOR
  { 0, 2, 3, 6, 7, 9, 11 },   // G_HARMONIC_MAJOR
  { 0, 1, 3, 4, 7, 8, 10 },   // G_SHARP_HARMONIC_MAJOR
  { 1, 2, 4, 5, 8, 9, 11 },   // A_HARMONIC_MAJOR
  { 0, 2, 3, 5, 6, 9, 10 },   // A_SHARP_HARMONIC_MAJOR
  { 1, 3, 4, 6, 7, 10, 11 },  // B_HARMONIC_MAJOR
  { 0, 2, 4, 6, 7, 9, 11 },   // C_LYDIAN
  { 0, 1, 3, 5, 7, 8, 10 },   // C_SHARP_LYDIAN
  { 1, 2, 4, 6, 8, 9, 11 },   // D_LYDIAN
  { 0, 2, 3, 5, 7, 9, 10 },   // D_SHARP_LYDIAN
  { 1, 3, 4, 6, 8, 10, 11 },  // E_LYDIAN
  { 0, 2, 4, 5, 7, 9, 11 },   // F_LYDIAN
  { 0, 1, 3, 5, 6, 8, 10 },   // F_SHARP_LYDIAN
  { 1, 2, 4, 6, 7, 9, 11 },   // G_LYDIAN
  { 0, 2, 3, 5, 7, 8, 10 },   // G_SHARP_LYDIAN
  { 1, 3, 4, 6, 8, 9, 11 },   // A_LYDIAN
  { 0, 2, 4, 5, 7, 9, 10 },   // A_SHARP_LYDIAN
  { 1, 3, 5, 6, 8, 10, 11 },  // B_LYDIAN
  { 0, 2, 3, 5, 7, 9, 10 },   // C_DORIAN
  { 1, 3, 4, 6, 8, 10, 11 },   // C_SHARP_DORIAN
  { 0, 2, 4, 5, 7, 9, 11 },    // D_DORIAN
  { 0, 1, 3, 5, 6, 8, 10 },    // D_SHARP_DORIAN
  { 1, 2 ,4, 6, 7, 9, 11},    // E_DORIAN
  { 0, 2, 3, 5, 7, 8, 10 },    // F_DORIAN
  { 1, 3, 4, 6, 8, 9, 11 },    // F_SHARP_DORIAN
  { 0, 2, 4, 5, 7, 9, 10 },    // G_DORIAN
  { 1, 3, 5, 6, 8, 10, 11 },   // G_SHARP_DORIAN
  { 0, 2, 4, 6, 7, 9, 11 },    // A_DORIAN
  { 0, 1, 3, 5, 7, 8, 10 },    // A_SHARP_DORIAN
  { 1, 2, 4, 6, 8, 9, 11 },    // B_DORIAN
  { 0, 1, 3, 5, 6, 8, 10 },   // C_LOCRIAN
  { 1, 2, 4, 6, 7, 9, 11 },   // C_SHARP_LOCRIAN
  { 0, 2, 3, 5, 7, 8, 10 },   // D_LOCRIAN
  { 1, 3, 4, 6, 8, 9, 11 },   // D_SHARP_LOCRIAN
  { 0, 2, 4, 5, 7, 9, 10 },   // E_LOCRIAN
  { 1, 3, 5, 6, 8, 10, 11 },  // F_LOCRIAN
  { 0, 2, 4, 6, 7, 9, 11 },   // F_SHARP_LOCRIAN
  { 0, 1, 3, 5, 7, 8, 10 },   // G_LOCRIAN
  { 1, 2, 4, 6, 8, 9, 11 },   // G_SHARP_LOCRIAN
  { 0, 2, 3, 5, 7, 9, 10 },   // A_LOCRIAN
  { 1, 3, 4, 6, 8, 10, 11 },  // A_SHARP_LOCRIAN
  { 0, 2, 4, 5, 7, 9, 11 }    // B_LOCRIAN

};

String scales_strings[] = {
  "Major",
  "Minor",
  "Harmonic Major",
  "Harmonic Minor",
  "Lydian",
  "Dorian",
  "Locrian"
};

String root_notes[] = {
  "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

byte getWhiteKeyIndex(byte noteInOctave) {
  switch (noteInOctave) {
    case 0: return 0;     // C
    case 2: return 1;     // D
    case 4: return 2;     // E
    case 5: return 3;     // F
    case 7: return 4;     // G
    case 9: return 5;     // A
    case 11: return 6;    // B
    default: return 255;  // Indicates black key or invalid note
  }
}

byte transpose(byte pitch) {
  byte noteInOctave = pitch % 12;
  byte octave = pitch / 12 * 12;
  byte index = getWhiteKeyIndex(noteInOctave);
  if (index == 255) return 0;  // Return 0 for black keys or invalid notes

  return octave + scales[currentScale][index];
}

// Use this function to change the scale type globally.
void setScaleType(ScaleType scale) {
  currentScale = scale;
}

byte handleNoteOnOff(byte channel, byte pitch, byte velocity, bool noteOn) {
  byte transposedPitch = 0;

  transposedPitch = transpose(pitch);

  // If it's a white key and pitch was transposed
  if (transposedPitch != 0) {
    if (noteOn) {
      MIDI.sendNoteOn(transposedPitch, velocity, channel);
    } else {
      MIDI.sendNoteOff(transposedPitch, velocity, channel);
    }
  }

  // Write the CV value to the DAC
  writeCvToDac(transposedPitch);

  return transposedPitch;  // Return the transposed pitch
}

void handleNoteOn(byte channel, byte pitch, byte velocity) {
  byte transposedPitch = handleNoteOnOff(channel, pitch, velocity, true);
  global_gate_tracker++;
  handleGate();  // Turn on the gate signal

  // If the pitch was transposed or it's a valid white key
  if (transposedPitch != 0 || (pitch % 12 == 0 || pitch % 12 == 2 || pitch % 12 == 4 || pitch % 12 == 5 || pitch % 12 == 7 || pitch % 12 == 9 || pitch % 12 == 11)) {
    String originalNote = midiNoteToName(pitch);
    String transposedNote = midiNoteToName(transposedPitch);

    Serial.print("Note On - Channel: ");
    Serial.print(channel);
    Serial.print(", Original Pitch: ");
    Serial.print(pitch);
    Serial.print(" (");
    Serial.print(originalNote);
    Serial.print("), Transposed Pitch: ");
    Serial.print(transposedPitch);
    Serial.print(" (");
    Serial.print(transposedNote);
    Serial.println(")");
  }
}

void handleNoteOff(byte channel, byte pitch, byte velocity) {
  byte transposedPitch = handleNoteOnOff(channel, pitch, velocity, false);
  global_gate_tracker--;
  handleGate();  // Turn off the gate signal
}

// Task for handling MIDI read
void TaskMIDIHandle(void* pvParameters) {
  (void)pvParameters;

  for (;;) {
    MIDI.read();
    vTaskDelay(1 / portTICK_PERIOD_MS);  // Necessary to allow task switch
  }
}

void TaskSerialHandle(void* pvParameters) {
  (void)pvParameters;
  String inputString;

  while (1) {
    if (Serial.available() > 0) {
      // Read the incoming data as a string
      inputString = Serial.readStringUntil('\n');

      // Remove any trailing newline or carriage return characters
      inputString.trim();

      if (inputString.equalsIgnoreCase("help")) 
      {
        // Provide a list of available commands
        Serial.println("Available Commands:");
        Serial.println("'help' - Shows this help message");
        Serial.println("'echo <text>' - Echoes back <text>");
        Serial.println("  major - Switch to C Major");
        Serial.println("  minor - Switch to C Minor");
        Serial.println("  harmonic minor - Switch to C Harmonic Minor");
        Serial.println("  harmonic major - Switch to C Harmonic Major");
        Serial.println("  MIDI Sync Controls: start, stop, bpm");
      }
      else if (inputString.equalsIgnoreCase("start")) {
        midiClockControl.running = true;
        Serial.println("MIDI clock started.");
      } 
      else if (inputString.equalsIgnoreCase("stop")) {
        midiClockControl.running = false;
        Serial.println("MIDI clock stopped.");
      } 
      else if (inputString.startsWith("bpm ")) {
        int newBpm = inputString.substring(4).toInt();
        if (newBpm > 0 && newBpm <= 300) {  // Validate BPM range
          midiClockControl.bpm = newBpm;
          Serial.print("BPM set to ");
          Serial.println(newBpm);
        } 
        else {
          Serial.println("Invalid BPM. Please enter a value between 1 and 300.");
        }
      } 
      else {
        // If command is not recognized, inform the user
        Serial.println("Command not recognized. Type 'help' for a list of commands.");
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Necessary to allow task switch
  }
}

void handle_root_key_input(String inputString)
{
  inputString.replace(" ", "_");
  inputString.toUpperCase();

  if (inputString == "C_MINOR") {
    setScaleType(C_MINOR);
  } else if (inputString == "C_SHARP_MINOR") {
    setScaleType(C_SHARP_MINOR);
  } else if (inputString == "D_MINOR") {
    setScaleType(D_MINOR);
  } else if (inputString == "C_HARMONIC_MINOR") {
    setScaleType(C_HARMONIC_MINOR);
  } else if (inputString == "C_SHARP_HARMONIC_MINOR") {
    setScaleType(C_SHARP_HARMONIC_MINOR);
  } else if (inputString == "D_HARMONIC_MINOR") {
    setScaleType(D_HARMONIC_MINOR);
  } else if (inputString == "C_MAJOR") {
    setScaleType(C_MAJOR);
  } else if (inputString == "C_SHARP_MAJOR") {
    setScaleType(C_SHARP_MAJOR);
  } else if (inputString == "D_MAJOR") {
    setScaleType(D_MAJOR);
  } else if (inputString == "C_HARMONIC_MAJOR") {
    setScaleType(C_HARMONIC_MAJOR);
  } else if (inputString == "C_SHARP_HARMONIC_MAJOR") {
    setScaleType(C_SHARP_HARMONIC_MAJOR);
  } else if (inputString == "D_HARMONIC_MAJOR") {
    setScaleType(D_HARMONIC_MAJOR);
  } else if (inputString == "D_SHARP_MINOR") {
    setScaleType(D_SHARP_MINOR);
  } else if (inputString == "E_MINOR") {
    setScaleType(E_MINOR);
  } else if (inputString == "F_MINOR") {
    setScaleType(F_MINOR);
  } else if (inputString == "F_SHARP_MINOR") {
    setScaleType(F_SHARP_MINOR);
  } else if (inputString == "G_MINOR") {
    setScaleType(G_MINOR);
  } else if (inputString == "G_SHARP_MINOR") {
    setScaleType(G_SHARP_MINOR);
  } else if (inputString == "A_MINOR") {
    setScaleType(A_MINOR);
  } else if (inputString == "A_SHARP_MINOR") {
    setScaleType(A_SHARP_MINOR);
  } else if (inputString == "B_MINOR") {
    setScaleType(B_MINOR);
  } else if (inputString == "D_SHARP_MAJOR") {
    setScaleType(D_SHARP_MAJOR);
  } else if (inputString == "E_MAJOR") {
    setScaleType(E_MAJOR);
  } else if (inputString == "F_MAJOR") {
    setScaleType(F_MAJOR);
  } else if (inputString == "F_SHARP_MAJOR") {
    setScaleType(F_SHARP_MAJOR);
  } else if (inputString == "G_MAJOR") {
    setScaleType(G_MAJOR);
  } else if (inputString == "G_SHARP_MAJOR") {
    setScaleType(G_SHARP_MAJOR);
  } else if (inputString == "A_MAJOR") {
    setScaleType(A_MAJOR);
  } else if (inputString == "A_SHARP_MAJOR") {
    setScaleType(A_SHARP_MAJOR);
  } else if (inputString == "B_MAJOR") {
    setScaleType(B_MAJOR);
  } else if (inputString == "D_SHARP_HARMONIC_MINOR") {
    setScaleType(D_SHARP_HARMONIC_MINOR);
  } else if (inputString == "E_HARMONIC_MINOR") {
    setScaleType(E_HARMONIC_MINOR);
  } else if (inputString == "F_HARMONIC_MINOR") {
    setScaleType(F_HARMONIC_MINOR);
  } else if (inputString == "F_SHARP_HARMONIC_MINOR") {
    setScaleType(F_SHARP_HARMONIC_MINOR);
  } else if (inputString == "G_HARMONIC_MINOR") {
    setScaleType(G_HARMONIC_MINOR);
  } else if (inputString == "G_SHARP_HARMONIC_MINOR") {
    setScaleType(G_SHARP_HARMONIC_MINOR);
  } else if (inputString == "A_HARMONIC_MINOR") {
    setScaleType(A_HARMONIC_MINOR);
  } else if (inputString == "A_SHARP_HARMONIC_MINOR") {
    setScaleType(A_SHARP_HARMONIC_MINOR);
  } else if (inputString == "B_HARMONIC_MINOR") {
    setScaleType(B_HARMONIC_MINOR);
  } else if (inputString == "D_SHARP_HARMONIC_MAJOR") {
    setScaleType(D_SHARP_HARMONIC_MAJOR);
  } else if (inputString == "E_HARMONIC_MAJOR") {
    setScaleType(E_HARMONIC_MAJOR);
  } else if (inputString == "F_HARMONIC_MAJOR") {
    setScaleType(F_HARMONIC_MAJOR);
  } else if (inputString == "F_SHARP_HARMONIC_MAJOR") {
    setScaleType(F_SHARP_HARMONIC_MAJOR);
  } else if (inputString == "G_HARMONIC_MAJOR") {
    setScaleType(G_HARMONIC_MAJOR);
  } else if (inputString == "G_SHARP_HARMONIC_MAJOR") {
    setScaleType(G_SHARP_HARMONIC_MAJOR);
  } else if (inputString == "A_HARMONIC_MAJOR") {
    setScaleType(A_HARMONIC_MAJOR);
  } else if (inputString == "A_SHARP_HARMONIC_MAJOR") {
    setScaleType(A_SHARP_HARMONIC_MAJOR);
  } else if (inputString == "B_HARMONIC_MAJOR") {
    setScaleType(B_HARMONIC_MAJOR);
  } else if (inputString == "C_LYDIAN") {
    setScaleType(C_LYDIAN);
  } else if (inputString == "C_SHARP_LYDIAN") {
    setScaleType(C_SHARP_LYDIAN);
  } else if (inputString == "D_LYDIAN") {
    setScaleType(D_LYDIAN);
  } else if (inputString == "D_SHARP_LYDIAN") {
    setScaleType(D_SHARP_LYDIAN);
  } else if (inputString == "E_LYDIAN") {
    setScaleType(E_LYDIAN);
  } else if (inputString == "F_LYDIAN") {
    setScaleType(F_LYDIAN);
  } else if (inputString == "F_SHARP_LYDIAN") {
    setScaleType(F_SHARP_LYDIAN);
  } else if (inputString == "G_LYDIAN") {
    setScaleType(G_LYDIAN);
  } else if (inputString == "G_SHARP_LYDIAN") {
    setScaleType(G_SHARP_LYDIAN);
  } else if (inputString == "A_LYDIAN") {
    setScaleType(A_LYDIAN);
  } else if (inputString == "A_SHARP_LYDIAN") {
    setScaleType(A_SHARP_LYDIAN);
  } else if (inputString == "B_LYDIAN") {
    setScaleType(B_LYDIAN);
  } else if (inputString == "C_DORIAN") {
    setScaleType(C_DORIAN);
  } else if (inputString == "C_SHARP_DORIAN") {
    setScaleType(C_SHARP_DORIAN);
  } else if (inputString == "D_DORIAN") {
    setScaleType(D_DORIAN);
  } else if (inputString == "D_SHARP_DORIAN") {
    setScaleType(D_SHARP_DORIAN);
  } else if (inputString == "E_DORIAN") {
    setScaleType(E_DORIAN);
  } else if (inputString == "F_DORIAN") {
    setScaleType(F_DORIAN);
  } else if (inputString == "F_SHARP_DORIAN") {
    setScaleType(F_SHARP_DORIAN);
  } else if (inputString == "G_DORIAN") {
    setScaleType(G_DORIAN);
  } else if (inputString == "G_SHARP_DORIAN") {
    setScaleType(G_SHARP_DORIAN);
  } else if (inputString == "A_DORIAN") {
    setScaleType(A_DORIAN);
  } else if (inputString == "A_SHARP_DORIAN") {
    setScaleType(A_SHARP_DORIAN);
  } else if (inputString == "B_DORIAN") {
    setScaleType(B_DORIAN);
  } else   if (inputString == "C_LOCRIAN") {
    setScaleType(C_LOCRIAN);
  } else if (inputString == "C_SHARP_LOCRIAN") {
    setScaleType(C_SHARP_LOCRIAN);
  } else if (inputString == "D_LOCRIAN") {
    setScaleType(D_LOCRIAN);
  } else if (inputString == "D_SHARP_LOCRIAN") {
    setScaleType(D_SHARP_LOCRIAN);
  } else if (inputString == "E_LOCRIAN") {
    setScaleType(E_LOCRIAN);
  } else if (inputString == "F_LOCRIAN") {
    setScaleType(F_LOCRIAN);
  } else if (inputString == "F_SHARP_LOCRIAN") {
    setScaleType(F_SHARP_LOCRIAN);
  } else if (inputString == "G_LOCRIAN") {
    setScaleType(G_LOCRIAN);
  } else if (inputString == "G_SHARP_LOCRIAN") {
    setScaleType(G_SHARP_LOCRIAN);
  } else if (inputString == "A_LOCRIAN") {
    setScaleType(A_LOCRIAN);
  } else if (inputString == "A_SHARP_LOCRIAN") {
    setScaleType(A_SHARP_LOCRIAN);
  } else if (inputString == "B_LOCRIAN") {
    setScaleType(B_LOCRIAN);
  }
}

// Add the CC handling function
void handleControlChange(byte channel, byte number, byte value) {
  // Send CC message directly through
  MIDI.sendControlChange(number, value, channel);
}

String getCurrentKeyModeString() {
  ;
}

void readEncoder() {
    static uint8_t state = 0;
    bool CLKstate = digitalRead(CLK);
    bool DTstate = digitalRead(DT);
    switch (state) {
        case 0:                         // Idle state, encoder not turning
            if (!CLKstate){             // Turn clockwise and CLK goes low first
                state = 1;
            } else if (!DTstate) {      // Turn anticlockwise and DT goes low first
                state = 4;
            }
            break;
        // Clockwise rotation
        case 1:                     
            if (!DTstate) {             // Continue clockwise and DT will go low after CLK
                state = 2;
            } 
            break;
        case 2:
            if (CLKstate) {             // Turn further and CLK will go high first
                state = 3;
            }
            break;
        case 3:
            if (CLKstate && DTstate) {  // Both CLK and DT now high as the encoder completes one step clockwise
                state = 0;
                ++inputDelta;
                printFlag = true;
            }
            break;
        // Anticlockwise rotation
        case 4:                         // As for clockwise but with CLK and DT reversed
            if (!CLKstate) {
                state = 5;
            }
            break;
        case 5:
            if (DTstate) {
                state = 6;
            }
            break;
        case 6:
            if (CLKstate && DTstate) {
                state = 0;
                --inputDelta;
                printFlag = true;
            }
            break; 
    }
}

void printDelta() {
    if (printFlag) {
        printFlag = false;
        Serial.println(inputDelta);
    }
}

void handlePitchBend(byte inChannel, int inValue) {
  MIDI.sendPitchBend(inValue, inChannel);
}

String midiNoteToName(byte note) {
  // An array of note names
  const char* noteNamesSharp[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
  const char* noteNamesFlat[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

  String noteName;
  byte octave = note / 12 - 1;  // MIDI note 0 is C-1

  // Choose to display sharps or flats. Here we arbitrarily choose sharps.
  noteName = noteNamesSharp[note % 12];

  // Append the octave number
  noteName += String(octave);

  return noteName;
}

// Function to write a CV value to the DAC
void writeCvToDac(byte input) {

  uint16_t mapValue = map(input, 0, 127, 0, 4095);

  // Write the level to the DAC
  dac.setVoltage(mapValue, false);
}

// Function to handle the gate signal
void handleGate() {
  digitalWrite(gatePin, global_gate_tracker ? HIGH : LOW);
}

// Task for handling MIDI clock

void TaskMidiClock(void* pvParameters) {
  const byte midiStartMsg = 0xFA;
  const byte midiStopMsg = 0xFC;
  const byte midiClockMsg = 0xF8;
  unsigned long lastTick = 0;
  const int baseTempo = 120000;  // 60,000 ms in a minute
  bool wasRunning = false;
  int clockCounter = 0;  // Counter for MIDI Clock messages

  MidiBeat currentBeat; // Define a MidiBeat for the current beat

  while (1) {
    if (midiClockControl.running) {
      if (!wasRunning) {
        MIDI.sendRealTime(midi::MidiType::Start);  // Send MIDI Start message
        wasRunning = true;
        // Initialize the currentBeat with the first note of the current scale
      }
      unsigned long currentMillis = millis();
      int interval = baseTempo / (midiClockControl.bpm * 24);  // 24 clock ticks per quarter note

      if (currentMillis - lastTick >= interval) {
        MIDI.sendRealTime(midi::MidiType::Clock);  // Send MIDI Clock message
                // Play the note only on the first of every 24 clock ticks
        clockCounter++;
        if (clockCounter % 24 == 0) {
          currentBeat.pitch = scales[currentScale][random(0, 7)] + (random(10,55) * 2); // You need to define this function
          currentBeat.velocity = 127;  // Example velocity
          currentBeat.channel = 16;     // Example channel
          // MIDI.sendNoteOn(currentBeat.pitch, currentBeat.velocity, currentBeat.channel);
          String SequencedNote = midiNoteToName(currentBeat.pitch);

          // Serial.print("Note On - Channel: ");
          // Serial.print(currentBeat.channel);
          // Serial.print(", Original Pitch: ");
          // Serial.print(currentBeat.pitch);
          // Serial.print(" (");
          // Serial.print(SequencedNote);
          // Serial.println(")");
        }
        lastTick = currentMillis;
      }
    } else {
      if (wasRunning) {
        MIDI.sendRealTime(midi::MidiType::Stop);  // Send MIDI Stop message
        wasRunning = false;
      }
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

//interrupt service routine for an encoder turn CC or CCW
void IRAM_ATTR ISR() 
{ 
  Serial.println("Encoder ISR");

}

void TaskScreenHandle(void* pvParameters) {
  int counter = 0;
  int currentStateCLK;
  int lastStateCLK;
  String currentDir ="";
  unsigned long lastButtonPress = 0;
  bool selectScale = true; // True to select scale, false to select root note

  String runningStatusString;
  String bpmString;  
  // Blinking related variables
  const unsigned long blinkInterval = 500; // 500 milliseconds for blinking
  unsigned long lastBlinkTime = 0;
  bool isBlinkVisible = true;

  lastStateCLK = digitalRead(CLK);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  display.clearDisplay();
  display.setRotation(2);
  display.setTextSize(1);               // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);  // Draw white text
  display.setCursor(0, 0);              // Start at top-left corner
  display.cp437(true);                  // Use full 256 char 'Code Page 437' font

  while (1) {
    // Read the button state
    int btnState = digitalRead(SW);

    //If we detect LOW signal, button is pressed
    if (btnState == LOW) {
      //if 50ms have passed since last LOW pulse, it means that the
      //button has been pressed, released and pressed again
      inputDelta = 0;
      if (millis() - lastButtonPress > 75) {
        selectMode = static_cast<SelectMode>((selectMode + 1) % 4); // Cycle through the selection modes
        Serial.print("Current Mode: ");
        Serial.print(selectMode);
        Serial.print(" | Running: ");
        Serial.print(midiClockControl.running);
        Serial.print(" | inputDelta: ");
        Serial.println(inputDelta);
      }

      // Remember last button press event
      lastButtonPress = millis();
    }

        // Time to toggle the blink state
    if (millis() - lastBlinkTime > blinkInterval) {
      isBlinkVisible = !isBlinkVisible;
      lastBlinkTime = millis();
    }

    // Display the concatenated string
    String displayString = root_notes[indexRoot] + " " + scales_strings[indexScale];
    handle_root_key_input(displayString);
    display.clearDisplay();

    // Calculate position and size for the root note and scale text
    int16_t x1, y1, x2, y2;
    uint16_t w1, h1, w2, h2;
    uint16_t w, h;
    display.getTextBounds(displayString, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (SCREEN_WIDTH - w) / 2;
    int16_t y = (SCREEN_HEIGHT - h) / 2;
    int spacing;
    int totalWidth;
    int startX;
    String rootNoteString;
    String scaleString;

    switch (selectMode) {
      case SELECT_ROOT:
        // Splitting the display string
        rootNoteString = root_notes[indexRoot];
        scaleString = scales_strings[indexScale];

        // Calculate text bounds for each part
        display.getTextBounds(rootNoteString, 0, 0, &x1, &y1, &w1, &h1);
        display.getTextBounds(scaleString, 0, 0, &x2, &y2, &w2, &h2);

        // Calculate overall width and starting position
        spacing = 5; // Adjust spacing between words as needed
        totalWidth = w1 + w2 + spacing;
        startX = (SCREEN_WIDTH - totalWidth) / 2;

        // Set cursor and print the root note
        display.setCursor(startX, y); // Adjust y as needed
        
        if (isBlinkVisible) {
        } else {
          // Do not display scale selection (blink not visible)
          display.print(rootNoteString);
        }
        // Set cursor and print the scale string
        display.setCursor(startX + w1 + spacing, y); // Adjust y as needed
        display.print(scaleString);

        // Display other items normally
        // Prepare the BPM string
        bpmString = "BPM: " + String(midiClockControl.bpm);

        // Calculate position for the BPM text (below the existing text)
        display.getTextBounds(bpmString, 0, 0, &x1, &y1, &w, &h);
        x = (SCREEN_WIDTH - w) / 2;
        y += h + 10;  // 10 pixels as a spacing between lines

        // Set cursor and print the BPM text
        display.setCursor(x, y);
        display.print(bpmString);

        // Prepare the Running Status string
        runningStatusString = midiClockControl.running ? "Running" : "Stopped";

        // Calculate position for the Running Status text (below the BPM text)
        display.getTextBounds(runningStatusString, 0, 0, &x1, &y1, &w, &h);
        x = (SCREEN_WIDTH - w) / 2;
        y = 0;  // Adjust the spacing as needed

        // Set cursor and print the Running Status text
        display.setCursor(x, y);
        display.print(runningStatusString);
        break;
      
      case SELECT_SCALE:
        // Splitting the display string
        rootNoteString = root_notes[indexRoot];
        scaleString = scales_strings[indexScale];

        // Calculate text bounds for each part
        display.getTextBounds(rootNoteString, 0, 0, &x1, &y1, &w1, &h1);
        display.getTextBounds(scaleString, 0, 0, &x2, &y2, &w2, &h2);

        // Calculate overall width and starting position
        spacing = 5; // Adjust spacing between words as needed
        totalWidth = w1 + w2 + spacing;
        startX = (SCREEN_WIDTH - totalWidth) / 2;

        // Set cursor and print the root note
        display.setCursor(startX, y); // Adjust y as needed
        
        display.print(rootNoteString);

        // Set cursor and print the scale string
        display.setCursor(startX + w1 + spacing, y); // Adjust y as needed
        if (isBlinkVisible) {
          display.print(scaleString);
        } else {
          // Do not display root selection (blink not visible)
        }
        // Display other items normally
        // Prepare the BPM string
        bpmString = "BPM: " + String(midiClockControl.bpm);

        // Calculate position for the BPM text (below the existing text)
        display.getTextBounds(bpmString, 0, 0, &x1, &y1, &w, &h);
        x = (SCREEN_WIDTH - w) / 2;
        y += h + 10;  // 10 pixels as a spacing between lines

        // Set cursor and print the BPM text
        display.setCursor(x, y);
        display.print(bpmString);

        // Prepare the Running Status string
        runningStatusString = midiClockControl.running ? "Running" : "Stopped";

        // Calculate position for the Running Status text (below the BPM text)
        display.getTextBounds(runningStatusString, 0, 0, &x1, &y1, &w, &h);
        x = (SCREEN_WIDTH - w) / 2;
        y = 0;  // Adjust the spacing as needed

        // Set cursor and print the Running Status text
        display.setCursor(x, y);
        display.print(runningStatusString);
        break;
      
      case ADJUST_BPM:
                // Splitting the display string
        rootNoteString = root_notes[indexRoot];
        scaleString = scales_strings[indexScale];

        // Calculate text bounds for each part
        display.getTextBounds(rootNoteString, 0, 0, &x1, &y1, &w1, &h1);
        display.getTextBounds(scaleString, 0, 0, &x2, &y2, &w2, &h2);

        // Calculate overall width and starting position
        spacing = 5; // Adjust spacing between words as needed
        totalWidth = w1 + w2 + spacing;
        startX = (SCREEN_WIDTH - totalWidth) / 2;

        // Set cursor and print the root note
        display.setCursor(startX, y); // Adjust y as needed
        display.print(rootNoteString);

        // Set cursor and print the scale string
        display.setCursor(startX + w1 + spacing, y); // Adjust y as needed
        display.print(scaleString);

        // Display other items normally
        // Prepare the BPM string
        bpmString = "BPM: " + String(midiClockControl.bpm);

        // Calculate position for the BPM text (below the existing text)
        display.getTextBounds(bpmString, 0, 0, &x1, &y1, &w, &h);
        x = (SCREEN_WIDTH - w) / 2;
        y += h + 10;  // 10 pixels as a spacing between lines

        // Set cursor and print the BPM text
        display.setCursor(x, y);
        if (isBlinkVisible) 
        {
          display.print(bpmString);
        } else {
          // Do not display scale selection (blink not visible)
        }

        // Prepare the Running Status string
        runningStatusString = midiClockControl.running ? "Running" : "Stopped";

        // Calculate position for the Running Status text (below the BPM text)
        display.getTextBounds(runningStatusString, 0, 0, &x1, &y1, &w, &h);
        x = (SCREEN_WIDTH - w) / 2;
        y = 0;  // Adjust the spacing as needed

        // Set cursor and print the Running Status text
        display.setCursor(x, y);
        display.print(runningStatusString);
        break;

      case RUNNING_MODE:
                // Splitting the display string
        rootNoteString = root_notes[indexRoot];
        scaleString = scales_strings[indexScale];

        // Calculate text bounds for each part
        display.getTextBounds(rootNoteString, 0, 0, &x1, &y1, &w1, &h1);
        display.getTextBounds(scaleString, 0, 0, &x2, &y2, &w2, &h2);

        // Calculate overall width and starting position
        spacing = 5; // Adjust spacing between words as needed
        totalWidth = w1 + w2 + spacing;
        startX = (SCREEN_WIDTH - totalWidth) / 2;

        // Set cursor and print the root note
        display.setCursor(startX, y); // Adjust y as needed
        display.print(rootNoteString);

        // Set cursor and print the scale string
        display.setCursor(startX + w1 + spacing, y); // Adjust y as needed
        display.print(scaleString);

        // Display other items normally
        // Prepare the BPM string
        bpmString = "BPM: " + String(midiClockControl.bpm);

        // Calculate position for the BPM text (below the existing text)
        display.getTextBounds(bpmString, 0, 0, &x1, &y1, &w, &h);
        x = (SCREEN_WIDTH - w) / 2;
        y += h + 10;  // 10 pixels as a spacing between lines

        // Set cursor and print the BPM text
        display.setCursor(x, y);
        display.print(bpmString);

        // Prepare the Running Status string
        runningStatusString = midiClockControl.running ? "Running" : "Stopped";

        // Calculate position for the Running Status text (below the BPM text)
        display.getTextBounds(runningStatusString, 0, 0, &x1, &y1, &w, &h);
        x = (SCREEN_WIDTH - w) / 2;
        y = 0;  // Adjust the spacing as needed

        // Set cursor and print the Running Status text
        display.setCursor(x, y);
        if (isBlinkVisible) 
        {
          display.print(runningStatusString);
        } else {
          // Do not display scale selection (blink not visible)
        }

        break;
    }

    // Refresh the display
    display.display();
  }
}

void setup() {
  Serial.begin(115200);

  vTaskDelay(1000/portTICK_PERIOD_MS);

  // Start the I2C bus
  Wire.begin();

  // Initialize encoder
  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT_PULLUP);

  // Begin communication with the DAC
  dac.begin(0x62);  // Replace with your MCP4725's address if different

  dac.setVoltage(1024, false);

  pinMode(gatePin, OUTPUT);

  Serial.println("MIDI C Major to C Minor Transposer Ready!");

  // attachInterrupt(CLK, ISR, FALLING); //setup encoder interrupts

  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleControlChange(handleControlChange);
  MIDI.setHandlePitchBend(handlePitchBend);
  MIDI.begin(MIDI_CHANNEL_OMNI);

  MIDI.turnThruOff();

  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(
    TaskMIDIHandle, /* Task function. */
    "TaskMIDI",     /* String with name of task. */
    10000,          /* Stack size in words. */
    NULL,           /* Parameter passed as input of the task */
    1,              /* Priority of the task. */
    NULL,           /* Task handle. */
    0);             /* Core where the task should run */

  xTaskCreatePinnedToCore(
    TaskScreenHandle, /* Task function. */
    "TaskScreen",     /* String with name of task. */
    10000,            /* Stack size in words. */
    NULL,             /* Parameter passed as input of the task */
    5,                /* Priority of the task. */
    NULL,             /* Task handle. */
    1);               /* Core where the task should run */

  xTaskCreatePinnedToCore(
    TaskSerialHandle, "TaskSerial", 10000, NULL, 5, NULL, 0);

  xTaskCreatePinnedToCore(TaskMidiClock, "MidiClockTask", 10000, NULL, 1, NULL, 0);

  Serial.println("Commands:");
  Serial.println("  major - Switch to C Major");
  Serial.println("  minor - Switch to C Minor");
  Serial.println("  harmonic minor - Switch to C Harmonic Minor");
  Serial.println("  harmonic major - Switch to C Harmonic Major");
  Serial.println("  MIDI Sync Controls: Start, Stop, BPM");
  Serial.println("  help - Show this help message");
}

void loop() {
  old_inputDelta = inputDelta;
  readEncoder();

  if (old_inputDelta != inputDelta)
  {
    switch (selectMode) {
      case SELECT_SCALE:
        indexScale = (inputDelta + sizeof(scales_strings)/sizeof(scales_strings[0])) % (sizeof(scales_strings)/sizeof(scales_strings[0]));
        break;
      case SELECT_ROOT:
        indexRoot = (inputDelta + sizeof(root_notes)/sizeof(root_notes[0])) % (sizeof(root_notes)/sizeof(root_notes[0]));
        break;
      case ADJUST_BPM:
        midiClockControl.bpm += inputDelta; // Adjust BPM value
        inputDelta = 0;
        break;
      case RUNNING_MODE:
        midiClockControl.running = inputDelta % 2;
          Serial.print("In loop(): Current Mode: ");
          Serial.print(selectMode);
          Serial.print(" | Running: ");
          Serial.print(midiClockControl.running);
          Serial.print(" | inputDelta: ");
          Serial.println(inputDelta);
        break;
      }
  }
}
