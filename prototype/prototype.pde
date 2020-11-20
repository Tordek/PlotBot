/* Config/calibration */ //<>//
float stepsPerMm = 20; // steps/mm
float coordScale = 100; // unitless
float precision = 5;   // mm
float a = 450; // Width of the coordinats.
float h = 300; // y offset

/* State */
PVector cartesianPosition = new PVector(); // mm. Represents the known position of the cursor.

/* Command info */
HashMap<Character, Float> memory = new HashMap();

/* Inner data */
BufferedReader reader;
PVector radialPosition = getRadiuses(cartesianPosition);  // steps. The machine doesn't know this value; this is the length of the rope.
float scale = 1; // pixels per mm
ArrayList<PVector> drawnPoints = new ArrayList<PVector>();
ArrayList<Character> validCharacters = new ArrayList<Character>();

// State relevant to the segment generation code
// It would be local to the function but it's global in order
// to be able to run the function over multiple steps.
boolean needsCommand = true;
float angle = 0;
float angleDelta = 0;
PVector radialTarget = new PVector(); // steps. Represents distance to new location.
PVector cartesianStep = new PVector();

void setup() {
  size(800, 600);
  reader = createReader("data.gcode");
  for (char c = 'A'; c < 'Z'; c++) {
    memory.put(c, 0f);
  }
  validCharacters.add('X');
  validCharacters.add('Y');
  validCharacters.add('Z');
  validCharacters.add('I');
  validCharacters.add('J');
  validCharacters.add('R');
  validCharacters.add('T');
}


void draw() {
  if (needsCommand) {
    parseLine();
  }

  calculateSegment();

  render();
}


/**
 * Takes a real world absolute position and converts to machine
 * absolute radiuses.
 */
PVector getRadiuses(PVector position) {
  PVector scaled = PVector.mult(position, coordScale);  
  float r1 = sqrt(pow(scaled.x + a, 2) + pow(h - scaled.y, 2));
  float r2 = sqrt(pow(scaled.x - a, 2) + pow(h - scaled.y, 2));

  return new PVector(r1, r2).mult(stepsPerMm);
}


/*
 * Takes a pair of absolute radiuses and converts to real world coordinates.
 * In theory p == fromRadiuses(getRadiuses(p));
 */
PVector fromRadiuses(PVector stepRadiuses) {  
  PVector radiuses = PVector.div(stepRadiuses, stepsPerMm);

  float x = pow(radiuses.x, 2) - pow(radiuses.y, 2);
  float y = sqrt(pow(4 * a * radiuses.x, 2) - pow(pow(radiuses.x, 2) - pow(radiuses.y, 2) + pow(2 * a, 2), 2));

  return new PVector(x, y).div(4 * a).add(0, -h).div(coordScale);
}


/*
 * Calculates the center of a circle of radius r passing through p1 and p2
 */
PVector circleCenter(PVector p1, PVector p2, float radius, boolean cw) {
  if (radius < 0) {
    radius = -radius;
    cw = !cw;
  }
  float q = p1.dist(p2);  
  PVector p3 = PVector.add(p2, p1).div(2);

  // abs is necessary when the points are 180 degrees apart
  // because q/2 ~= radius and should be 0 but ends up as a tiny negative.
  PVector offset = new PVector(
    sqrt(abs(pow(radius, 2) - pow(q/2, 2))) * (p1.y - p2.y) / q, 
    sqrt(abs(pow(radius, 2) - pow(q/2, 2))) * (p2.x - p1.x) / q);
  if (cw) {
    return p3.add(offset);
  } else {
    return p3.sub(offset);
  }
}

float readFloat(BufferedReader r) {
  String s = "";
  while (true) {
    try {
      r.mark(32);
      char c = (char)r.read();

      if (!Character.isWhitespace(c)) {
        r.reset();
        break;
      }
    } 
    catch (IOException e) {
    }
  }
  while (true) {
    try {
      r.mark(32);
      char c = (char)r.read();

      if (Character.isWhitespace(c)) {
        r.reset();
        break;
      }

      if (c != '-') {
        try {
          Float.parseFloat(s + c);
        } 
        catch (NumberFormatException e) {
          r.reset();
          break;
        }
      }

      s = s + c;
    } 
    catch (IOException e) {
    }
  }  

  return Float.parseFloat(s);
}

void render() {
  background(224);
  // Put 0,0 at the center.
  translate(width/2, height/2); 
  // 
  scale(scale);

  strokeWeight(1.0/scale);

  // Ropes
  fill(255, 0, 0);
  stroke(0);
  line(-a, -h, cartesianPosition.x * coordScale, -cartesianPosition.y * coordScale);
  line(a, -h, cartesianPosition.x * coordScale, -cartesianPosition.y * coordScale);

  noFill();
  stroke(0);
  // Cursor (rebuilt from radial)
  circle(-a, -h, radialPosition.x * 2 / stepsPerMm);
  circle(a, -h, radialPosition.y * 2 / stepsPerMm);


  // Center
  // circle(0, 0, 10);
  stroke(0);
  strokeWeight(2.0/scale);
  // Object
  for (PVector point : drawnPoints) {
    point(point.x * coordScale, point.y * coordScale);
  }
  stroke(255, 0, 0);
  point(memory.get('X') * coordScale, -memory.get('Y') * coordScale);
}

void parseLine() {  
  // Parse the command
  boolean calculateCenter = false;
  boolean calculateRadius = false;
  
  try {
    while (true) {
      int pc = reader.read();
      if (pc == -1) {
        break;
      }

      char c = (char) pc;

      if (c == '\n') {
        // EOL
        break;
      }

      if (c == '%') {
        // Start of program
        continue;
      }

      if (c == '(') {
        // Comment
        while ((char) reader.read() != ')');
        continue;
      }

      if (Character.isWhitespace(c)) {
        continue;
      }

      if (!Character.isUpperCase(c)) {
        throw new Exception("!! Unknown character: " + c);
      }

      float value = readFloat(reader);
      if (c == 'G') {
        // We only handle G0-3
        if (value == 0 || value == 1 || value == 2 || value == 3) {            
          memory.put(c, value);
          continue;
        } else if (value == 21) { // This code only handles millimeters.
          continue;
        }
      } else if (validCharacters.contains(c)) {
        memory.put(c, value);
        if (c == 'R') {
          calculateCenter = true;
        }
        if (c == 'I' || c == 'J') {
          calculateRadius = true;
        }
        continue;
      }
      
      System.err.println("!! Unhandled word: " + c + Float.toString(value));      
    }
  }
  catch (Exception e) {
    e.printStackTrace();
    exit();
  }

  // Command preparation.
  radialTarget.set(0, 0);
  if (memory.get('G') == 1) {
    cartesianStep
      .set(memory.get('X'), memory.get('Y'))
      .sub(cartesianPosition)
      .normalize()
      .mult(precision / coordScale);
  } else if (memory.get('G') == 2 || memory.get('G') == 3) {
    if (calculateCenter) {
      // Calculate IJ
      PVector arcCenter = circleCenter(cartesianPosition, new PVector(memory.get('X'), memory.get('Y')), memory.get('R'), memory.get('G') == 2);
      memory.put('I', arcCenter.x);
      memory.put('J', arcCenter.y);
      memory.put('R', abs(memory.get('R')));
    }

    if (calculateRadius) {
      memory.put('R', new PVector(memory.get('I'), memory.get('J')).mag());
      memory.put('I', memory.get('I') + cartesianPosition.x);
      memory.put('J', memory.get('J') + cartesianPosition.y);
    }

    angle = PVector.sub(cartesianPosition, new PVector(memory.get('I'), memory.get('J'))).heading();
    angleDelta = precision / coordScale / memory.get('R');
    if (memory.get('G') == 2) {
      angleDelta = -angleDelta;
    }
  }

  needsCommand = false;
}

void calculateSegment() {  
  // Calculate the target radiuses.
  PVector intermediateCartesian = new PVector(memory.get('X'), memory.get('Y'));

  boolean lastStep = false;
  if (intermediateCartesian.dist(cartesianPosition) * coordScale < precision) { // Fudge the precision a tiny bit to avoid corner cases
    lastStep = true;
  } else if (memory.get('G') == 1) {
    // Direction vector from start point
    intermediateCartesian.set(cartesianPosition).add(cartesianStep);
  } else if (memory.get('G') == 2 || memory.get('G') == 3) {
    angle += angleDelta;
    intermediateCartesian = PVector.fromAngle(angle).mult(memory.get('R')).add(memory.get('I'), memory.get('J'));
  }

  radialTarget.add(getRadiuses(intermediateCartesian));  
  radialTarget.sub(getRadiuses(cartesianPosition));
  cartesianPosition.set(intermediateCartesian);

  // Move toward the target radiuses    
  PVector direction = new PVector(Math.signum(radialTarget.x), Math.signum(radialTarget.y));

  PVector absoluteRadialTarget = new PVector(abs(radialTarget.x), abs(radialTarget.y));

  if (absoluteRadialTarget.x > absoluteRadialTarget.y) {
    float ratio = absoluteRadialTarget.x / absoluteRadialTarget.y;
    float stepsy = 0;
    for (float stepsx = 0; stepsx < absoluteRadialTarget.x; stepsx++) {
      radialPosition.x += direction.x;        
      radialTarget.x -= direction.x;
      if (stepsy * ratio < stepsx) {
        radialPosition.y += direction.y;
        radialTarget.y -= direction.y;
        stepsy++;
      }
    }
  } else {
    float ratio = absoluteRadialTarget.y / absoluteRadialTarget.x;
    float stepsx = 0;
    for (float stepsy = 0; stepsy < absoluteRadialTarget.y; stepsy++) {
      radialPosition.y += direction.y;
      radialTarget.y -= direction.y;
      if (stepsx * ratio < stepsy) {
        radialPosition.x += direction.x;
        radialTarget.x -= direction.x;
        stepsx++;
      }
    }
  }

  drawnPoints.add(fromRadiuses(radialPosition));

  if (lastStep) {
    needsCommand = true;          
    System.out.println("ok");
  }
}
