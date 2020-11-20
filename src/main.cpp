#include <Arduino.h>

#define PIN_DIR0 7
#define PIN_DIR1 13
#define PIN_STEP0 4
#define PIN_STEP1 12
#define ENABLE 8

/* Calibration */
// Steps of the motor per mm of string
const float stepsPerMm = 53;
// Separation of the centers.
const float a = 450.0f;
// Y offset
const float h = 620.0f;
// Square of the distance from the endpoints over which to accelerate
const float accelRatio = pow(stepsPerMm * 5, 2);
// Longest of each line step
const float precision = 1.0f;
// Zoom factor for models (equivalent to multiplying the numbers in the G-code).
const float scaleFactor = 1;

/* State */
float cartesianPositionX = 0.0f;
float cartesianPositionY = 0.0f;

// Command
float values[26];

float get(char address)
{
  return values[address - 'A'];
}

void set(char address, float value)
{
  // TODO: Handle special commands like G21 to set inches
  values[address - 'A'] = value;
}

// Math helpers
float mag(float x, float y)
{
  return sqrt(pow(x, 2) + pow(y, 2));
}

float distance(float p1X, float p1Y, float p2X, float p2Y)
{
  return mag(p1X - p2X, p1Y - p2Y);
}

// Convert IRL units to radial positioning.
float getRadius0(float cartesianX, float cartesianY)
{
  return mag(cartesianX * scaleFactor + a, h - cartesianY * scaleFactor) * stepsPerMm;
}

float getRadius1(float cartesianX, float cartesianY)
{
  return mag(cartesianX * scaleFactor - a, h - cartesianY * scaleFactor) * stepsPerMm;
}

// Move each motor by one step.
void step0()
{
  digitalWrite(PIN_STEP0, 0);
  delayMicroseconds(5);
  digitalWrite(PIN_STEP0, 1);
}

void step1()
{
  digitalWrite(PIN_STEP1, 0);
  delayMicroseconds(5);
  digitalWrite(PIN_STEP1, 1);
}

/**
 * Calculate the center of the circle passing through p1 and p2 with radius r.
 * Will return the center of the circle with the smallest arc distance between both points
 * unless r is negative.
 */
void calculateCircleCenter(float p1x, float p1y, float p2x, float p2y, float r, bool clockwise, float &arcCenterX, float &arcCenterY)
{
  if (r < 0)
  {
    r = -r;
    clockwise = !clockwise;
  }

  const float q = distance(p1x, p1y, p2x, p2y);
  const float p3x = (p1x + p2x) / 2;
  const float p3y = (p1y + p2y) / 2;

  // ABS is necessary because in certain edge cases (angle=180) the
  // value can become negative
  const float offsetx = sqrt(abs(pow(r, 2) - pow(q / 2, 2))) * (p1y - p2y) / q;
  const float offsety = sqrt(abs(pow(r, 2) - pow(q / 2, 2))) * (p2x - p1x) / q;

  if (clockwise)
  {
    arcCenterX = p3x + offsetx;
    arcCenterY = p3y + offsety;
  }
  else
  {
    arcCenterX = p3x - offsetx;
    arcCenterY = p3y - offsety;
  }
}

bool isConfigValue(int c)
{
  switch (c)
  {
  case 'X':
  case 'Y':
  case 'Z':
  case 'I':
  case 'J':
  case 'R':
  case 'T':
    return true;
  }

  return false;
}

void setup()
{
  Serial.begin(9600);
  Serial.println("( Starting )");

  pinMode(PIN_DIR0, OUTPUT);
  pinMode(PIN_DIR1, OUTPUT);
  pinMode(PIN_STEP0, OUTPUT);
  pinMode(PIN_STEP1, OUTPUT);
  pinMode(ENABLE, OUTPUT);
  digitalWrite(ENABLE, 1);

  set('F', 6000); // mm/min; 100mm/s
}

void loop()
{
  Serial.println("ok");
  // TODO: Most words aren't modal, make those settings temporary.
  bool calculateCenter = false;
  bool calculateRadius = false;

  // Read and parse command
  Serial.print(">>> ");
  while (true)
  {
    while (Serial.available() < 1)
      ;
    const int c = Serial.read();
    if (c == -1)
    {
      return;
    }

    if (c == '\n')
    {
      // EOL
      break;
    }

    if (c == '%')
    {
      // Start of program
      continue;
    }

    if (c == '(')
    {
      // Discard comment.
      while (Serial.read() != ')')
        ;
      continue;
    }

    if (isspace(c))
    {
      continue;
    }

    if (!isupper(c))
    {
      Serial.print("!! Invalid character: ");
      Serial.println(c);
    }

    float value = Serial.parseFloat();
    if (c == 'G')
    {
      if (value == 0 || value == 1 || value == 2 || value == 3)
      {
        // Movement commands
        set(c, value);
        continue;
      }
      else if (value == 21)
      {
        // Set machine to mm
        continue;
      }
    }
    else if (isConfigValue(c))
    {
      if (c == 'I' || c == 'J')
      {
        calculateRadius = true;
      }

      if (c == 'R')
      {
        calculateCenter = true;
      }
      set(c, value);
      continue;
    }

    Serial.print("!! Unhandled command: ");
    Serial.print((char) c);
    Serial.println(value);
  }

  // Some command preparation
  // The extremes for the command, used for acceleration.
  const float radialStart0 = getRadius0(cartesianPositionX, cartesianPositionY);
  const float radialStart1 = getRadius1(cartesianPositionX, cartesianPositionY);
  const float radialEnd0 = getRadius0(get('X'), get('Y'));
  const float radialEnd1 = getRadius1(get('X'), get('Y'));

  // For linear interpolation
  float cartesianStepX;
  float cartesianStepY;

  // For circular interpolation
  float arcCenterX;
  float arcCenterY;
  float angle;
  float angleDelta;
  float radius;

  if (get('G') == 1)
  {
    cartesianStepX = get('X') - cartesianPositionX;
    cartesianStepY = get('Y') - cartesianPositionY;
    float magnitude = distance(0, 0, cartesianStepX, cartesianStepY);
    cartesianStepX *= precision / scaleFactor / magnitude;
    cartesianStepY *= precision / scaleFactor / magnitude;
  }
  else if (get('G') == 2 || get('G') == 3)
  {
    if (calculateCenter)
    {
      // Calculate IJ
      calculateCircleCenter(cartesianPositionX, cartesianPositionY,
                            get('X'), get('Y'),
                            get('R'),
                            get('G') == 2,
                            arcCenterX, arcCenterY);
      radius = abs(get('R'));
    }

    if (calculateRadius)
    {
      arcCenterX = get('I');
      arcCenterY = get('J');
      radius = mag(arcCenterX, arcCenterY);
      arcCenterX += cartesianPositionX;
      arcCenterY += cartesianPositionY;
    }

    angle = atan2(cartesianPositionY - arcCenterY, cartesianPositionX - arcCenterX);
    angleDelta = precision / scaleFactor / radius;
    if (get('G') == 2)
    {
      angleDelta = -angleDelta;
    }
  }

  // Movement
  digitalWrite(ENABLE, 0);
  bool done = false;
  // How much the head needs to move. Carries the error.
  float radialTarget0 = 0;
  float radialTarget1 = 0;

  while (!done)
  {
    // Aim at the target
    float intermediateCartesianX = get('X');
    float intermediateCartesianY = get('Y');
    const float magnitude = distance(cartesianPositionX, cartesianPositionY, intermediateCartesianX, intermediateCartesianY);

    if (magnitude < precision / scaleFactor)
    {
      // If the target is too close, just go.
      done = true;
    }
    else if (get('G') == 1)
    {
      // Build a linear segment
      intermediateCartesianX = cartesianPositionX + cartesianStepX;
      intermediateCartesianY = cartesianPositionY + cartesianStepY;
    }
    else if (get('G') == 2 || get('G') == 3)
    {
      // Build a circular segment
      angle += angleDelta;
      intermediateCartesianX = cos(angle) * radius + arcCenterX;
      intermediateCartesianY = sin(angle) * radius + arcCenterY;
    }

    // Current position
    float radialPosition0 = getRadius0(cartesianPositionX, cartesianPositionY);
    float radialPosition1 = getRadius1(cartesianPositionX, cartesianPositionY);

    // Add to the radial target to carry the rounding error forward.
    radialTarget0 += getRadius0(intermediateCartesianX, intermediateCartesianY) - radialPosition0;
    radialTarget1 += getRadius1(intermediateCartesianX, intermediateCartesianY) - radialPosition1;

    // After this movement we'll be there.
    cartesianPositionX = intermediateCartesianX;
    cartesianPositionY = intermediateCartesianY;

    // Calculate speed delay (5us for pulse).
    const float speedDelay = 200; // (60 * 1000000) / (stepsPerMm * get('F')) - 5;

    // Set direction
    digitalWrite(PIN_DIR0, radialTarget0 < 0);
    digitalWrite(PIN_DIR1, radialTarget1 > 0);

    const float absRadialTarget0 = abs(radialTarget0);
    const float absRadialTarget1 = abs(radialTarget1);
    const float radialDirection0 = radialTarget0 > 1 ? 1 : -1;
    const float radialDirection1 = radialTarget1 > 1 ? 1 : -1;

    if (absRadialTarget0 > absRadialTarget1)
    {
      const float ratio = absRadialTarget0 / absRadialTarget1;
      float steps1 = 0;
      for (float steps0 = 0; steps0 < absRadialTarget0; steps0++)
      {
        step0();
        radialPosition0 += radialDirection0;
        radialTarget0 -= radialDirection0;

        if (steps1 * ratio < steps0)
        {
          step1();
          steps1++;
          radialPosition1 += radialDirection1;
          radialTarget1 -= radialDirection1;
        }

        // The closest the cursor is to the start or the end, the slower it moves.
        float accelDistanceSquared = accelRatio;
        accelDistanceSquared = fmin(accelDistanceSquared, pow(radialPosition0 - radialStart0, 2) + pow(radialPosition1 - radialStart1, 2));
        accelDistanceSquared = fmin(accelDistanceSquared, pow(radialPosition0 - radialEnd0, 2) + pow(radialPosition1 - radialEnd1, 2));
        delayMicroseconds(speedDelay + (accelRatio - accelDistanceSquared) / 50);
      }
    }
    else
    {
      const float ratio = absRadialTarget1 / absRadialTarget0;
      float steps0 = 0;
      for (float steps1 = 0; steps1 < absRadialTarget1; steps1++)
      {
        step1();
        radialTarget1 -= radialDirection1;
        radialPosition1 += radialDirection1;

        if (steps0 * ratio < steps1)
        {
          step0();
          steps0++;
          radialTarget0 -= radialDirection0;
          radialPosition0 += radialDirection0;
        }

        // The closest the cursor is to the start or the end, the slower it moves.
        float accelDistanceSquared = accelRatio;
        accelDistanceSquared = fmin(accelDistanceSquared, pow(radialPosition0 - radialStart0, 2) + pow(radialPosition1 - radialStart1, 2));
        accelDistanceSquared = fmin(accelDistanceSquared, pow(radialPosition0 - radialEnd0, 2) + pow(radialPosition1 - radialEnd1, 2));
        delayMicroseconds(speedDelay + (accelRatio - accelDistanceSquared) / 50);
      }
    }
  }
  digitalWrite(ENABLE, 1);
}
