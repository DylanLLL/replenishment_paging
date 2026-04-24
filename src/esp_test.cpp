// SKETCH FOR PERIPHERALS TESTING

#include <Arduino.h>
#define TEST_GROUND_UNIT 1
#define TEST_FLOOR_UNIT 0

#if TEST_FLOOR_UNIT
#define NUM_SENSORS 6

// TRIG and ECHO pin arrays
const int trigPins[NUM_SENSORS] = { 4, 17, 18, 22, 33, 26 };
const int echoPins[NUM_SENSORS] = { 16, 5, 19, 23, 25, 27 };

long duration[NUM_SENSORS];
float distance_cm[NUM_SENSORS];

const int BUTTON_PIN = 32;
const int LED_PIN = 13;
const int BUZZER_PIN = 14;

void setup()
{
  Serial.begin(115200);

  for (int i = 0; i < NUM_SENSORS; i++)
  {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
  }

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("Floor Unit Test");
}

float readUltrasonic(int trigPin, int echoPin)
{
  // Clear trigger
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Send 10us pulse
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read echo with timeout (30ms = ~5m range)
  long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0)
  {
    return -1;  // No reading
  }

  return duration * 0.034 / 2;
}

void loop()
{
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    distance_cm[i] = readUltrasonic(trigPins[i], echoPins[i]);

    Serial.print("Sensor ");
    Serial.print(i);
    Serial.print(": ");

    if (distance_cm[i] < 0)
    {
      Serial.print("Out of range");
    }
    else
    {
      Serial.print(distance_cm[i]);
      Serial.print(" cm");
    }

    Serial.print(" | ");

    delay(60);  // IMPORTANT: avoid cross-talk between sensors
  }

  Serial.println();
  delay(200);

  if (digitalRead(BUTTON_PIN) == LOW)
  {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.println("Button pressed: LED ON, Buzzer ON");
  }
  else
  {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("Button released: LED OFF, Buzzer OFF");
  }
}
#endif

#if TEST_GROUND_UNIT

const int BUTTON_FLOOR_1 = 4;
const int BUTTON_FLOOR_2 = 16;
const int BUTTON_FLOOR_3 = 17;
const int BUTTON_FLOOR_4 = 5;
const int LED_1 = 33;
const int LED_2 = 25;
const int LED_3 = 26;
const int LED_4 = 27;
const int BUZZER_PIN = 13;

void buzzer1()
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(100);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(100);
}
void buzzer2()
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
  delay(200);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
  delay(200);
}
void buzzer3()
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(BUZZER_PIN, LOW);
  delay(300);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(BUZZER_PIN, LOW);
  delay(300);
}
void buzzer4()
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(400);
  digitalWrite(BUZZER_PIN, LOW);
  delay(400);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(400);
  digitalWrite(BUZZER_PIN, LOW);
  delay(400);
}

void setup()
{
  Serial.begin(115200);
  pinMode(BUTTON_FLOOR_1, INPUT_PULLUP);
  pinMode(BUTTON_FLOOR_2, INPUT_PULLUP);
  pinMode(BUTTON_FLOOR_3, INPUT_PULLUP);
  pinMode(BUTTON_FLOOR_4, INPUT_PULLUP);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(LED_3, OUTPUT);
  pinMode(LED_4, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("Ground Unit Test");
}

void loop()
{
  if (digitalRead(BUTTON_FLOOR_1) == LOW)
  {
    digitalWrite(LED_1, HIGH);
    buzzer1();
    Serial.println("Button 1 pressed: LED 1 ON, Buzzer 1");
    digitalWrite(LED_1, LOW);
  }
  if (digitalRead(BUTTON_FLOOR_2) == LOW)
  {
    digitalWrite(LED_2, HIGH);
    buzzer2();
    Serial.println("Button 2 pressed: LED 2 ON, Buzzer 2");
    digitalWrite(LED_2, LOW);
  }
  if (digitalRead(BUTTON_FLOOR_3) == LOW)
  {
    digitalWrite(LED_3, HIGH);
    buzzer3();
    Serial.println("Button 3 pressed: LED 3 ON, Buzzer 3");
    digitalWrite(LED_3, LOW);
  }
  if (digitalRead(BUTTON_FLOOR_4) == LOW)
  {
    digitalWrite(LED_4, HIGH);
    buzzer4();
    Serial.println("Button 4 pressed: LED 4 ON, Buzzer 4");
    digitalWrite(LED_4, LOW);
  }

  delay(500);
}

#endif
