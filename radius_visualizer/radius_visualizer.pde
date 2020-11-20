PVector c1p = new PVector(0, 0);
PVector c2p = new PVector(800, 0);

void setup() {
  size(800, 600);
}

void draw() {  
  PVector mousePos = new PVector(mouseX, mouseY);
  float c1r = mousePos.dist(c1p);
  float c2r = mousePos.dist(c2p);
  
  background(224);
  noFill();
  stroke(128);
  circle(c1p.x, c1p.y, 2 * c1r);
  circle(c2p.x, c2p.y, 2 * c2r);
  stroke(0);
  line(c1p.x, c1p.y, mouseX, mouseY);
  line(c2p.x, c2p.y, mouseX, mouseY);
}
