#define FSR_PIN 34


int threshold = 2000;   

const unsigned long tapGapMax = 700;
const unsigned long requiredPause = 500;
const unsigned long resetTimeout = 2000;


bool tapDetected = false;

int stage = 0;
int tapCount = 0;

unsigned long lastTapTime = 0;

int filteredValue = 0;
int lastStableValue = 0;


int readFSR()
{
    long sum = 0;

    for(int i = 0; i < 10; i++)
    {
        sum += analogRead(FSR_PIN);
        delay(2);
    }

    int rawValue = sum / 10;

    filteredValue = (0.7 * filteredValue) + (0.3 * rawValue);

    lastStableValue = filteredValue;

    return filteredValue;
}

void resetPattern()
{
    stage = 0;
    tapCount = 0;

    Serial.println("Pattern Reset");
}


void setup()
{
    Serial.begin(115200);

    analogReadResolution(12);


}


void loop()
{
    int fsrValue = readFSR();

    unsigned long currentTime = millis();

    // Serial.print("FSR Value: ");
    // Serial.println(fsrValue);

    if(fsrValue > threshold && !tapDetected)
    {
        tapDetected = true;

        Serial.println("TAP DETECTED");

        // Reset if too much time between taps
        if(currentTime - lastTapTime > resetTimeout)
        {
            resetPattern();
        }

        // -------- STAGE 0 --------
        // Detect first 3 taps
        if(stage == 0)
        {
            tapCount++;

            Serial.print("Stage 0 Tap Count: ");
            Serial.println(tapCount);

            if(tapCount == 3)
            {
                stage = 1;

                Serial.println("3 Taps Completed");
                Serial.println("Waiting For Pause...");
            }
        }

        // -------- STAGE 1 --------
        // Detect pause before next 2 taps
        else if(stage == 1)
        {
            unsigned long pauseTime = currentTime - lastTapTime;

            if(pauseTime > requiredPause)
            {
                stage = 2;
                tapCount = 1;

                Serial.println("Pause Accepted");
                Serial.println("Stage 2 Started");
            }
            else
            {
                Serial.println("Pause Too Short");

                resetPattern();
            }
        }

        // -------- STAGE 2 --------
        // Detect final 2 taps
        else if(stage == 2)
        {
            if(currentTime - lastTapTime < tapGapMax)
            {
                tapCount++;

                Serial.print("Stage 2 Tap Count: ");
                Serial.println(tapCount);

                if(tapCount == 2)
                {
                    Serial.println("");
                    Serial.println("################################");
                    Serial.println("######## SOS DETECTED #########");
                    Serial.println("################################");
                    Serial.println("");


                    delay(2000);
                    resetPattern();
                }
            }
            else
            {
                Serial.println("Tap Gap Too Long");

                resetPattern();
            }
        }

        // Store latest tap time
        lastTapTime = currentTime;
    }


    if(fsrValue < 1200 && (millis() - lastTapTime > 80))
    {
        tapDetected = false;
    }

    delay(20);
}