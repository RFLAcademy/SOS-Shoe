#define FSR_PIN 3

int threshold = 1200;

bool tapDetected = false;

int readFSR()
{
    long sum = 0;

    for(int i = 0; i < 10; i++)
    {
        sum += analogRead(FSR_PIN);
        delay(2);
    }

    return sum / 10;
}

void setup()
{
    Serial.begin(115200);

    // Set ADC resolution
    analogReadResolution(12);

    Serial.println("FSR Test Started");
}

void loop()
{
    int fsrValue = readFSR();

    Serial.print("FSR Value: ");
    Serial.println(fsrValue);

    // Tap Detection
    if(fsrValue > threshold && !tapDetected)
    {
        Serial.println("TAP DETECTED!");
        tapDetected = true;
    }

    // Reset detection
    if(fsrValue < 1000)
    {
        tapDetected = false;
    }

    delay(20);
}