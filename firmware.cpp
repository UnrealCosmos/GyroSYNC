#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SDA_PIN 21
#define SCL_PIN 22

#define BUTTON_PIN 15

#define DOUBLE_PRESS_TIME 40

#define MPU6050_ADDR 0x68

#define PWR_MGMT_1  0x6B
#define GYRO_CONFIG 0x1B
#define GYRO_XOUT_H 0x43


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SH1106G display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);


// One gyro reading
#define SAMPLE_INTERVAL 100

// 10 samples = approximately 1 sec
#define NUM_SAMPLES 10

float gxSamples[NUM_SAMPLES];
float gySamples[NUM_SAMPLES];
float gzSamples[NUM_SAMPLES];

int sampleIndex = 0;

float avgGx = 0;
float avgGy = 0;
float avgGz = 0;


float refGx = 0;
float refGy = 0;
float refGz = 0;

bool originSet = false;


float originalGx = 0;
float originalGy = 0;
float originalGz = 0;

bool lastButtonState = HIGH;

bool waitingForSecondPress = false;

unsigned long firstPressTime = 0;


String statusMessage = "BTN = ZERO";

unsigned long statusTime = 0;

#define STATUS_DISPLAY_TIME 1000



void writeRegister(byte reg, byte value)
{
  Wire.beginTransmission(MPU6050_ADDR);

  Wire.write(reg);
  Wire.write(value);

  Wire.endTransmission();
}


int16_t read16()
{
  int16_t high = Wire.read();
  int16_t low  = Wire.read();

  return (high << 8) | low;
}



void readGyro(float &gx, float &gy, float &gz)
{
  Wire.beginTransmission(MPU6050_ADDR);

  Wire.write(GYRO_XOUT_H);

  Wire.endTransmission(false);

  Wire.requestFrom(MPU6050_ADDR, 6);

  if (Wire.available() == 6)
  {
    int16_t rawGx = read16();
    int16_t rawGy = read16();
    int16_t rawGz = read16();

    // ±250 °/s
    // Sensitivity = 131 LSB/(°/s)
    

    gx = rawGx / 131.0;
    gy = rawGy / 131.0;
    gz = rawGz / 131.0;
  }
}



float calculateAverage(float values[])
{
  float sum = 0;

  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    sum += values[i];
  }

  return sum / NUM_SAMPLES;
}

void handleButton()
{
  bool buttonState = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && buttonState == LOW)
  {
    unsigned long currentTime = millis();

    if (waitingForSecondPress &&
        (currentTime - firstPressTime <= DOUBLE_PRESS_TIME))
    {

      refGx = originalGx;
      refGy = originalGy;
      refGz = originalGz;

      originSet = true;

      waitingForSecondPress = false;

      statusMessage = "ORIGINAL RESET";
      statusTime = millis();

      Serial.println();
      Serial.println("==============================");
      Serial.println("       DOUBLE PRESS");
      Serial.println("    ORIGINAL RESTORED");
      Serial.println("==============================");

      Serial.print("Original Gx: ");
      Serial.println(originalGx, 2);

      Serial.print("Original Gy: ");
      Serial.println(originalGy, 2);

      Serial.print("Original Gz: ");
      Serial.println(originalGz, 2);
    }


    else
    {
      firstPressTime = currentTime;

      waitingForSecondPress = true;
    }
  }


  if (waitingForSecondPress &&
      (millis() - firstPressTime > DOUBLE_PRESS_TIME))
  {
    


    refGx = avgGx;
    refGy = avgGy;
    refGz = avgGz;

    originSet = true;

    waitingForSecondPress = false;

    statusMessage = "NEW ZERO SET";
    statusTime = millis();

    Serial.println();
    Serial.println("==============================");
    Serial.println("        SINGLE PRESS");
    Serial.println("       NEW ZERO SET");
    Serial.println("==============================");

    Serial.print("New Reference Gx: ");
    Serial.println(refGx, 2);

    Serial.print("New Reference Gy: ");
    Serial.println(refGy, 2);

    Serial.print("New Reference Gz: ");
    Serial.println(refGz, 2);
  }

  lastButtonState = buttonState;
}




void setup()
{
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);


  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(0x3C, true))
  {
    Serial.println("OLED NOT FOUND!");

    while (1)
    {
      delay(10);
    }
  }

  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("ADAPTIVE GYRO");

  display.setCursor(0, 20);
  display.println("Initializing...");

  display.display();

  delay(1000);
  writeRegister(PWR_MGMT_1, 0x00);

  writeRegister(GYRO_CONFIG, 0x00);

  Serial.println("MPU6050 READY");

  float gx;
  float gy;
  float gz;

  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    readGyro(gx, gy, gz);

    gxSamples[i] = gx;
    gySamples[i] = gy;
    gzSamples[i] = gz;

    delay(SAMPLE_INTERVAL);
  }

  // ===================================================
  // CALCULATE ORIGINAL STARTUP REFERENCE
  // ===================================================

  originalGx = calculateAverage(gxSamples);
  originalGy = calculateAverage(gySamples);
  originalGz = calculateAverage(gzSamples);

  // Initially current reference = startup reference
  refGx = originalGx;
  refGy = originalGy;
  refGz = originalGz;

  avgGx = originalGx;
  avgGy = originalGy;
  avgGz = originalGz;

  originSet = true;

  // ===================================================
  // READY SCREEN
  // ===================================================

  display.clearDisplay();

  display.setCursor(0, 0);
  display.println("ADAPTIVE GYRO");

  display.setCursor(0, 20);
  display.println("MPU6050 READY");

  display.setCursor(0, 35);
  display.println("1x = NEW ZERO");

  display.setCursor(0, 50);
  display.println("2x = ORIGINAL");

  display.display();

  delay(2000);
}



void loop()
{
  static unsigned long lastSampleTime = 0;


  handleButton();

  if (millis() - lastSampleTime >= SAMPLE_INTERVAL)
  {
    lastSampleTime = millis();


    float gx;
    float gy;
    float gz;

    readGyro(gx, gy, gz);


    gxSamples[sampleIndex] = gx;
    gySamples[sampleIndex] = gy;
    gzSamples[sampleIndex] = gz;

    sampleIndex++;

    if (sampleIndex >= NUM_SAMPLES)
    {
      sampleIndex = 0;
    }


    avgGx = calculateAverage(gxSamples);
    avgGy = calculateAverage(gySamples);
    avgGz = calculateAverage(gzSamples);


    float relativeGx = avgGx - refGx;
    float relativeGy = avgGy - refGy;
    float relativeGz = avgGz - refGz;


    Serial.print("Gx: ");
    Serial.print(relativeGx, 2);

    Serial.print(" °/s    Gy: ");
    Serial.print(relativeGy, 2);

    Serial.print(" °/s    Gz: ");
    Serial.print(relativeGz, 2);

    Serial.println(" °/s");

    // OLEd

    display.clearDisplay();

    display.setTextSize(1);


    display.setCursor(0, 0);
    display.println("ADAPTIVE GYRO");

    display.drawLine(
      0,
      10,
      127,
      10,
      SH110X_WHITE
    );


    display.setCursor(0, 19);

    display.print("Gx: ");
    display.print(relativeGx, 1);
    display.print(" d/s");


    display.setCursor(0, 32);

    display.print("Gy: ");
    display.print(relativeGy, 1);
    display.print(" d/s");

    display.setCursor(0, 45);

    display.print("Gz: ");
    display.print(relativeGz, 1);
    display.print(" d/s");
    // STATUS
    
    display.setCursor(0, 58);

    if (millis() - statusTime < STATUS_DISPLAY_TIME)
    {
      display.print(statusMessage);
    }
    else
    {
      display.print("1x ZERO  2x RESET");
    }

    display.display();
  }
}