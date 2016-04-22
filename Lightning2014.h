//  Lightning2014 is a program that lets you make an LED strip
//  look like a bolt of lightning
//
//  Daniel Wilson, 2014
//  https://github.com/fibonacci162/LEDs
//
//  With BIG thanks to the FastLED community!

#define FREQUENCY     50                // controls the interval between strikes
#define FLASHES       8                 // the upper limit of flashes per strike

unsigned int dimmer = 1;

// The first "flash" in a bolt of lightning is the "leader." The leader
// is usually duller and has a longer delay until the next flash. Subsequent
// flashes, the "strokes," are brighter and happen at shorter intervals.

uint32_t lightning2014()
{
    for (int flashCounter = 0; flashCounter < random8(3, FLASHES); flashCounter++)
    {
        if (flashCounter == 0) dimmer = 5;     // the brightness of the leader is scaled down by a factor of 5
        else dimmer = random8(1, 3);           // return strokes are brighter than the leader

        FastLED.showColor(CHSV(255, 0, 255 / dimmer));
        delay(random8(4, 10));                 // each flash only lasts 4-10 milliseconds
        FastLED.showColor(CHSV(255, 0, 0));

        if (flashCounter == 0) delay(150);   // longer delay until next flash after the leader
        delay(50 + random8(100));               // shorter delay between strokes  
    }

    fill_solid(leds, NUM_LEDS - 1, CRGB::Black);

    return random8(FREQUENCY) * 100;          // delay between strikes
}
