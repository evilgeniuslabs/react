byte sequence[255];

bool reset = true;

CRGB colors[4] = {
    CRGB::Red,
    CRGB::Green,
    CRGB::Blue,
    CRGB::White,
};

uint16_t resetDelay = 1000;
uint16_t sequenceDelay = 500;
uint16_t sequencePauseDelay = 250;

bool playingSequence = true;
bool pausingBetweenSequence = true;
bool pausingBeforeSequence = true;

byte sequenceLength = 255;
byte sequenceIndex = 0;

uint32_t simon()
{
    random16_add_entropy(random());

    if (reset) {
        for (int i = 0; i < 255; i++) {
            sequence[i] = random8(0, 4);
        }

        sequenceLength = 1;
        sequenceIndex = 0;

        playingSequence = true;
        pausingBetweenSequence = true;

        reset = false;
        
        fill_solid(leds, NUM_LEDS, CRGB::Black);

        return resetDelay;
    }

    if (sequenceIndex >= sequenceLength) {
        playingSequence = false;
        pausingBetweenSequence = false;
        sequenceIndex = 0;
    }

    if(pausingBeforeSequence) {
        pausingBeforeSequence = false;
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        return sequenceDelay;
    }

    if (pausingBetweenSequence) {
        pausingBetweenSequence = false;
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        return sequencePauseDelay;
    }

    byte sequenceColorIndex = sequence[sequenceIndex];
    CRGB sequenceColor = colors[sequenceColorIndex];

    if (playingSequence) {
        sequenceIndex++;
        fill_solid(leds, NUM_LEDS, sequenceColor);
        pausingBetweenSequence = true;
        return sequenceDelay;
    }

    bool buttonPressed = false;
    CRGB colorPressed = CRGB::Black;

    switch (command) {
        case InputCommand::Red:
            buttonPressed = true;
            colorPressed = CRGB::Red;
            break;

        case InputCommand::Green:
            buttonPressed = true;
            colorPressed = CRGB::Green;
            break;

        case InputCommand::Blue:
            buttonPressed = true;
            colorPressed = CRGB::Blue;
            break;

        case InputCommand::White:
            buttonPressed = true;
            colorPressed = CRGB::White;
            break;

        case InputCommand::None:
        default:
            buttonPressed = false;
            break;
    }

    if (!buttonPressed) {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        return 0;
    }

    fill_solid(leds, NUM_LEDS, sequenceColor);

    if (colorPressed == sequenceColor) {
        if (sequenceIndex + 1 == sequenceLength) {
            sequenceLength++;
            sequenceIndex = 0;
            playingSequence = true;
            pausingBetweenSequence = true;
            pausingBeforeSequence = true;
            return sequencePauseDelay;
        }
        else {
            sequenceIndex++;
            return sequencePauseDelay;
        }
    }
    else {
        reset = true;
        return resetDelay;
    }
}