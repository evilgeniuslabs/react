/*
 * React: https://github.com/evilgeniuslabs/react
 * Copyright (C) 2015 Jason Coon, Evil Genius Labs
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <SerialFlash.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#include <FastLED.h>
#include <IRremote.h>
#include <EEPROM.h>

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define LED_PIN     3
#define CLOCK_PIN   2
#define IR_RECV_PIN 12
#define COLOR_ORDER GBR
#define CHIPSET     APA102
#define NUM_LEDS    126

AudioInputAnalog         input(A8);
AudioAnalyzeFFT256       fft;
AudioConnection          audioConnection(input, 0, fft, 0);

const uint8_t brightnessCount = 5;
uint8_t brightnessMap[brightnessCount] = { 16, 32, 64, 128, 255 };
uint8_t brightness = brightnessMap[0];

CRGB leds[NUM_LEDS];
IRrecv irReceiver(IR_RECV_PIN);

CRGB solidColor = CRGB::Red;

#include "Commands.h"

typedef uint32_t(*PatternFunctionPointer)();
typedef PatternFunctionPointer PatternList [];
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

CRGBPalette16 gPalette;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

int autoPlayDurationSeconds = 10;
unsigned int autoPlayTimout = 0;
bool autoplayEnabled = false;

InputCommand command;

int currentIndex = 0;
PatternFunctionPointer currentPattern;

#include "SoftTwinkles.h"
#include "Fire2012WithPalette.h"
#include "BouncingBalls2014.h"
#include "Lightning2014.h"
#include "ColorTwinkles.h"
#include "Spectrum.h"
#include "DiscoStrobe.h"
#include "ColorInvaders.h"
#include "Simon.h"
#include "ColorWaves.h"

const PatternList patterns = {
    waves,
    colorWaves,
    pride,
    softTwinkles,
    colorTwinkles,
    fire2012WithPalette,
    sinelon,
    lightning2014,
    bouncingBalls2014,
    spectrumBar,
    spectrumDots,
    juggle,
    bpm,
    confetti,
    rainbow,
    rainbowWithGlitter,
    hueCycle,
    discostrobe,
    simon,
    colorInvaders,
    showSolidColor,
};

const int patternCount = ARRAY_SIZE(patterns);

void setup()
{
    delay(500); // sanity delay
    Serial.println("setup start");

    loadSettings();

    FastLED.addLeds<CHIPSET, LED_PIN, CLOCK_PIN, COLOR_ORDER, DATA_RATE_MHZ(12)>(leds, NUM_LEDS);
//    FastLED.setCorrection(TypicalLEDStrip); // 0xB0FFFF
    FastLED.setBrightness(brightness);

    // Initialize the IR receiver
    irReceiver.enableIRIn();
    irReceiver.blink13(true);

    gPalette = HeatColors_p;

    currentPattern = patterns[currentIndex];

    autoPlayTimout = millis() + (autoPlayDurationSeconds * 1000);

    // Serial.print("reading... ");
    // randomSeed(analogRead(A8));
    // Serial.println("done");

    // Audio requires memory to work.
    AudioMemory(12);

    Serial.println("setup end");
}

void loop()
{
    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy(random());

    unsigned int requestedDelay = currentPattern();

    FastLED.show(); // display this frame

    handleInput(requestedDelay);

    if (autoplayEnabled && millis() > autoPlayTimout) {
        move(1);
        autoPlayTimout = millis() + (autoPlayDurationSeconds * 1000);
    }

    // do some periodic updates
    EVERY_N_MILLISECONDS(20) { gHue++; } // slowly cycle the "base color" through the rainbow
}

void loadSettings() {
    // load settings from EEPROM

    // brightness
    brightness = EEPROM.read(0);
    if (brightness < 1)
        brightness = 1;
    else if (brightness > 255)
        brightness = 255;

    // currentIndex
    currentIndex = EEPROM.read(1);
    if (currentIndex < 0)
        currentIndex = 0;
    else if (currentIndex >= patternCount)
        currentIndex = patternCount - 1;

    // solidColor
    solidColor.r = EEPROM.read(2);
    solidColor.g = EEPROM.read(3);
    solidColor.b = EEPROM.read(4);

    if (solidColor.r == 0 && solidColor.g == 0 && solidColor.b == 0)
        solidColor = CRGB::White;
}

void setSolidColor(CRGB color) {
    solidColor = color;

    EEPROM.write(2, solidColor.r);
    EEPROM.write(3, solidColor.g);
    EEPROM.write(4, solidColor.b);

    moveTo(patternCount - 1);
}

void powerOff()
{
    // clear the display
    // fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
        FastLED.show(); // display this frame
        delay(1);
    }

    FastLED.show(); // display this frame

    while (true) {
        InputCommand command = readCommand();
        if (command == InputCommand::Power ||
            command == InputCommand::Brightness)
            return;

        // go idle for a while, converve power
        delay(250);
    }
}

void move(int delta) {
    moveTo(currentIndex + delta);
}

void moveTo(int index) {
    currentIndex = index;

    if (currentIndex >= patternCount)
        currentIndex = 0;
    else if (currentIndex < 0)
        currentIndex = patternCount - 1;

    currentPattern = patterns[currentIndex];

    fill_solid(leds, NUM_LEDS, CRGB::Black);

    for (int i = 0; i < NUM_LEDS; i++) {
        heat[i] = 0;
    }

    EEPROM.write(1, currentIndex);
}

int getBrightnessLevel() {
    int level = 0;
    for (int i = 0; i < brightnessCount; i++) {
        if (brightnessMap[i] >= brightness) {
            level = i;
            break;
        }
    }
    return level;
}

uint8_t cycleBrightness() {
    adjustBrightness(1);

    if (brightness == brightnessMap[0])
        return 0;

    return brightness;
}

void adjustBrightness(int delta) {
    int level = getBrightnessLevel();

    level += delta;
    if (level < 0)
        level = brightnessCount - 1;
    if (level >= brightnessCount)
        level = 0;

    brightness = brightnessMap[level];
    FastLED.setBrightness(brightness);

    EEPROM.write(0, brightness);
}

void handleInput(unsigned int requestedDelay) {
    unsigned int requestedDelayTimeout = millis() + requestedDelay;

    while (true) {
        command = readCommand(defaultHoldDelay);

        if (command != InputCommand::None) {
            Serial.print("command: ");
            Serial.println((int) command);
        }

        if (command == InputCommand::Up) {
            move(1);
            break;
        }
        else if (command == InputCommand::Down) {
            move(-1);
            break;
        }
        else if (command == InputCommand::Brightness) {
            if (isHolding || cycleBrightness() == 0) {
                heldButtonHasBeenHandled();
                powerOff();
                break;
            }
        }
        else if (command == InputCommand::Power) {
            powerOff();
            break;
        }
        else if (command == InputCommand::BrightnessUp) {
            adjustBrightness(1);
        }
        else if (command == InputCommand::BrightnessDown) {
            adjustBrightness(-1);
        }
        else if (command == InputCommand::PlayMode) { // toggle pause/play
            autoplayEnabled = !autoplayEnabled;
        }
        //else if (command == InputCommand::Palette) { // cycle color pallete
        //    effects.CyclePalette();
        //}

        // pattern buttons

        else if (command == InputCommand::Pattern1) {
            moveTo(0);
            break;
        }
        else if (command == InputCommand::Pattern2) {
            moveTo(1);
            break;
        }
        else if (command == InputCommand::Pattern3) {
            moveTo(2);
            break;
        }
        else if (command == InputCommand::Pattern4) {
            moveTo(3);
            break;
        }
        else if (command == InputCommand::Pattern5) {
            moveTo(4);
            break;
        }
        else if (command == InputCommand::Pattern6) {
            moveTo(5);
            break;
        }
        else if (command == InputCommand::Pattern7) {
            moveTo(6);
            break;
        }
        else if (command == InputCommand::Pattern8) {
            moveTo(7);
            break;
        }
        else if (command == InputCommand::Pattern9) {
            moveTo(8);
            break;
        }
        else if (command == InputCommand::Pattern10) {
            moveTo(9);
            break;
        }
        else if (command == InputCommand::Pattern11) {
            moveTo(10);
            break;
        }
        else if (command == InputCommand::Pattern12) {
            moveTo(11);
            break;
        }

        // custom color adjustment buttons

        else if (command == InputCommand::RedUp) {
            solidColor.red += 1;
            setSolidColor(solidColor);
            break;
        }
        else if (command == InputCommand::RedDown) {
            solidColor.red -= 1;
            setSolidColor(solidColor);
            break;
        }
        else if (command == InputCommand::GreenUp) {
            solidColor.green += 1;
            setSolidColor(solidColor);

            break;
        }
        else if (command == InputCommand::GreenDown) {
            solidColor.green -= 1;
            setSolidColor(solidColor);
            break;
        }
        else if (command == InputCommand::BlueUp) {
            solidColor.blue += 1;
            setSolidColor(solidColor);
            break;
        }
        else if (command == InputCommand::BlueDown) {
            solidColor.blue -= 1;
            setSolidColor(solidColor);
            break;
        }

        // color buttons

        else if (command == InputCommand::Red && currentIndex != patternCount - 2 && currentIndex != patternCount - 3) { // Red, Green, and Blue buttons can be used by ColorInvaders game, which is the next to last pattern
            setSolidColor(CRGB::Red);
            break;
        }
        else if (command == InputCommand::RedOrange) {
            setSolidColor(CRGB::OrangeRed);
            break;
        }
        else if (command == InputCommand::Orange) {
            setSolidColor(CRGB::Orange);
            break;
        }
        else if (command == InputCommand::YellowOrange) {
            setSolidColor(CRGB::Goldenrod);
            break;
        }
        else if (command == InputCommand::Yellow) {
            setSolidColor(CRGB::Yellow);
            break;
        }

        else if (command == InputCommand::Green && currentIndex != patternCount - 2 && currentIndex != patternCount - 3) { // Red, Green, and Blue buttons can be used by ColorInvaders game, which is the next to last pattern
            setSolidColor(CRGB::Green);
            break;
        }
        else if (command == InputCommand::Lime) {
            setSolidColor(CRGB::Lime);
            break;
        }
        else if (command == InputCommand::Aqua) {
            setSolidColor(CRGB::Aqua);
            break;
        }
        else if (command == InputCommand::Teal) {
            setSolidColor(CRGB::Teal);
            break;
        }
        else if (command == InputCommand::Navy) {
            setSolidColor(CRGB::Navy);
            break;
        }

        else if (command == InputCommand::Blue && currentIndex != patternCount - 2 && currentIndex != patternCount - 3) { // Red, Green, and Blue buttons can be used by ColorInvaders game, which is the next to last pattern
            setSolidColor(CRGB::Blue);
            break;
        }
        else if (command == InputCommand::RoyalBlue) {
            setSolidColor(CRGB::RoyalBlue);
            break;
        }
        else if (command == InputCommand::Purple) {
            setSolidColor(CRGB::Purple);
            break;
        }
        else if (command == InputCommand::Indigo) {
            setSolidColor(CRGB::Indigo);
            break;
        }
        else if (command == InputCommand::Magenta) {
            setSolidColor(CRGB::Magenta);
            break;
        }

        else if (command == InputCommand::White && currentIndex != patternCount - 2 && currentIndex != patternCount - 3) {
            setSolidColor(CRGB::White);
            break;
        }
        else if (command == InputCommand::Pink) {
            setSolidColor(CRGB::Pink);
            break;
        }
        else if (command == InputCommand::LightPink) {
            setSolidColor(CRGB::LightPink);
            break;
        }
        else if (command == InputCommand::BabyBlue) {
            setSolidColor(CRGB::CornflowerBlue);
            break;
        }
        else if (command == InputCommand::LightBlue) {
            setSolidColor(CRGB::LightBlue);
            break;
        }

        if (millis() >= requestedDelayTimeout)
            break;
    }
}

// scale the brightness of the screenbuffer down
void dimAll(byte value)
{
  for (uint8_t i = 0; i < NUM_LEDS; i++){
    leds[i].nscale8(value);
  }
}

uint32_t showSolidColor() {
    fill_solid(leds, NUM_LEDS, solidColor);

    return 60;
}

uint32_t waves()
{
  static uint8_t theta = 0;

  gPalette = OceanColors_p;

  for ( uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = sin8(theta + i);
    leds[i] = CHSV(160, 255, j);

    leds[(NUM_LEDS - 1) - i] += CHSV(160, 255, j / 2);
  }

  EVERY_N_MILLISECONDS(30) { theta++; }

  return 0;
}

uint32_t rainbow()
{
    // FastLED's built-in rainbow generator
    fill_rainbow(leds, NUM_LEDS, gHue, 7);

    return 8;
}

uint32_t rainbowWithGlitter()
{
    // built-in FastLED rainbow, plus some random sparkly glitter
    rainbow();
    addGlitter(80);
    return 8;
}

void addGlitter(fract8 chanceOfGlitter)
{
    if (random8() < chanceOfGlitter) {
        leds[random16(NUM_LEDS)] += CRGB::White;
    }
}

uint32_t confetti()
{
    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy(leds, NUM_LEDS, 10);
    int pos = random16(NUM_LEDS);
    leds[pos] += CHSV(gHue + random8(64), 200, 255);
    return 8;
}

uint32_t bpm()
{
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    uint8_t BeatsPerMinute = 62;
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    for (int i = 0; i < NUM_LEDS; i++) { //9948
        leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
    }
    return 8;
}

uint32_t juggle() {
    // eight colored dots, weaving in and out of sync with each other
    fadeToBlackBy(leds, NUM_LEDS, 20);
    byte dothue = 0;
    for (int i = 0; i < 8; i++) {
        leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
    return 8;
}

// An animation to play while the crowd goes wild after the big performance
uint32_t applause()
{
    static uint16_t lastPixel = 0;
    fadeToBlackBy(leds, NUM_LEDS, 32);
    leds[lastPixel] = CHSV(random8(HUE_BLUE, HUE_PURPLE), 255, 255);
    lastPixel = random16(NUM_LEDS);
    leds[lastPixel] = CRGB::White;
    return 8;
}

// An "animation" to just fade to black.  Useful as the last track
// in a non-looping performance-oriented playlist.
uint32_t fadeToBlack()
{
    fadeToBlackBy(leds, NUM_LEDS, 10);
    return 8;
}

uint32_t sinelon()
{
    // a colored dot sweeping back and forth, with fading trails
    fadeToBlackBy(leds, NUM_LEDS, 20);
    int pos = beatsin16(13, 0, NUM_LEDS);
    leds[pos] += CHSV(gHue, 255, 192);
    return 8;
}

uint32_t hueCycle() {
    fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
    return 60;
}

// Pride2015 by Mark Kriegsman
// https://gist.github.com/kriegsman/964de772d64c502760e5

// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
uint32_t pride()
{
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;//gHue * 256;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for (int i = 0; i < NUM_LEDS; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t) ((uint32_t) b16 * (uint32_t) b16) / 65536;
        uint8_t bri8 = (uint32_t) (((uint32_t) bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        uint8_t pixelnumber = i;
        pixelnumber = (NUM_LEDS - 1) - pixelnumber;

        nblend(leds[pixelnumber], newcolor, 64);
    }

    return 0;
}
