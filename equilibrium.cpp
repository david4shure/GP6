////////////////////////////////////////////////////////////////////////
//
//   Model a system with dynamic equilibrium.  Uses Phong lighting,
//   time-based animation, and homemade geometry objects.
//
//   This skeleton code demos the use of a time-based animation clock.
//
//   Westmont College
//   CS150 : 3D Computer Graphics
//   Professor David Hunter
//
//   Some code is from  _Foundations of 3D Computer Graphics_
//   by Steven Gortler.  See AUTHORS file for more details.
//
//
//   
//   
////////////////////////////////////////////////////////////////////////

#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#if __GNUG__
#   include <tr1/memory>
#endif

#include <GL/glew.h>
#ifdef __MAC__
#   include <GLUT/glut.h>
#else
#   include <GL/glut.h>
#endif

#include "cvec.h"
#include "matrix4.h"
#include "geometrymaker.h"
#include "ppm.h"
#include "glsupport.h"
#include "bezier.h"

using namespace std; // for string, vector, iostream, and other standard C++ stuff
using namespace tr1; // for shared_ptr

// G L O B A L S ///////////////////////////////////////////////////

// --------- IMPORTANT --------------------------------------------------------
// Before you start working on this assignment, set the following variable
// properly to indicate whether you want to use OpenGL 2.x with GLSL 1.0 or
// OpenGL 3.x+ with GLSL 1.3.
//
// Set g_Gl2Compatible = true to use GLSL 1.0 and g_Gl2Compatible = false to
// use GLSL 1.3. Make sure that your machine supports the version of GLSL you
// are using. In particular, on Mac OS X currently there is no way of using
// OpenGL 3.x with GLSL 1.3 when GLUT is used.
//
// If g_Gl2Compatible=true, shaders with -gl2 suffix will be loaded.
// If g_Gl2Compatible=false, shaders with -gl3 suffix will be loaded.
// To complete the assignment you only need to edit the shader files that get
// loaded
// ----------------------------------------------------------------------------
static const bool g_Gl2Compatible = false;

static float g_frustMinFov = 60.0;        // Show at least 60 degree field of view
static float g_frustFovY = g_frustMinFov; // FOV in y direction (updated by updateFrustFovY)

static const float g_frustNear = -0.1;    // near plane
static const float g_frustFar = -50.0;    // far plane

static int g_windowWidth = 512;
static int g_windowHeight = 512;
static bool g_mouseClickDown = false;    // is the mouse button pressed
static bool g_mouseLClickButton, g_mouseRClickButton, g_mouseMClickButton;
static int g_mouseClickX, g_mouseClickY; // coordinates for mouse click event
static int g_activeShader = 1;
static int g_objToManip = 1;  // object to manipulate 

  // Animation globals for time-based animation
static const float g_animStart = 0.0;
static const float g_animMax = 1.0; 
static float g_animClock = g_animStart; // clock parameter runs from g_animStart to g_animMax then repeats
static float g_animSpeed = 0.5;         // clock units per second
static int g_elapsedTime = 0;           // keeps track of how long it takes between frames
static float g_animIncrement = g_animSpeed/60.0; // updated by idle() based on GPU speed
static string viewPoint = "s";
static bool drawCurve = false;


struct ShaderState {
  GlProgram program;

  // Handles to uniform variables
  GLint h_uLight, h_uLight2; // two lights
  GLint h_uProjMatrix;
  GLint h_uModelViewMatrix;
  GLint h_uNormalMatrix;
  GLint h_uColor;

  // Handles to vertex attributes
  GLint h_aPosition;
  GLint h_aNormal;

  ShaderState(const char* vsfn, const char* fsfn) {
    readAndCompileShader(program, vsfn, fsfn);

    const GLuint h = program; // short hand

    // Retrieve handles to uniform variables
    h_uLight = safe_glGetUniformLocation(h, "uLight");
    h_uLight2 = safe_glGetUniformLocation(h, "uLight2");
    h_uProjMatrix = safe_glGetUniformLocation(h, "uProjMatrix");
    h_uModelViewMatrix = safe_glGetUniformLocation(h, "uModelViewMatrix");
    h_uNormalMatrix = safe_glGetUniformLocation(h, "uNormalMatrix");
    h_uColor = safe_glGetUniformLocation(h, "uColor");

    // Retrieve handles to vertex attributes
    h_aPosition = safe_glGetAttribLocation(h, "aPosition");
    h_aNormal = safe_glGetAttribLocation(h, "aNormal");

    if (!g_Gl2Compatible)
      glBindFragDataLocation(h, 0, "fragColor");
    checkGlErrors();
  }
};

static const int g_numShaders = 2;
static const char * const g_shaderFiles[g_numShaders][2] = {
  {"./shaders/basic-gl3.vshader", "./shaders/solid-gl3.fshader"},
  {"./shaders/basic-gl3.vshader", "./shaders/phong-gl3.fshader"}
};
static const char * const g_shaderFilesGl2[g_numShaders][2] = {
  {"./shaders/basic-gl2.vshader", "./shaders/solid-gl2.fshader"},
  {"./shaders/basic-gl2.vshader", "./shaders/phong-gl2.fshader"}
};
static vector<shared_ptr<ShaderState> > g_shaderStates; // our global shader states

// --------- Geometry

// Macro used to obtain relative offset of a field within a struct
#define FIELD_OFFSET(StructType, field) &(((StructType *)0)->field)

// A vertex with floating point Position, Normal, and one set of teXture coordinates;
struct VertexPNX {
  Cvec3f p, n; // position and normal vectors
  Cvec2f x; // texture coordinates

  VertexPNX() {}

  VertexPNX(float x, float y, float z,
            float nx, float ny, float nz,
            float u, float v)
    : p(x,y,z), n(nx, ny, nz), x(u, v) 
  {}

  VertexPNX(const Cvec3f& pos, const Cvec3f& normal, const Cvec2f& texCoords)
    :  p(pos), n(normal), x(texCoords) {}

  VertexPNX(const Cvec3& pos, const Cvec3& normal, const Cvec2& texCoords)
    : p(pos[0], pos[1], pos[2]), n(normal[0], normal[1], normal[2]), x(texCoords[0], texCoords[1]) {}

  // Define copy constructor and assignment operator from GenericVertex so we can
  // use make* function templates from geometrymaker.h
  VertexPNX(const GenericVertex& v) {
    *this = v;
  }

  VertexPNX& operator = (const GenericVertex& v) {
    p = v.pos;
    n = v.normal;
    x = v.tex;
    return *this;
  }
};

struct Geometry {
  GlBufferObject vbo, ibo;
  int vboLen, iboLen;

  Geometry(VertexPNX *vtx, unsigned short *idx, int vboLen, int iboLen) {
    this->vboLen = vboLen;
    this->iboLen = iboLen;

    // Now create the VBO and IBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPNX) * vboLen, vtx, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * iboLen, idx, GL_STATIC_DRAW);
  }

  void draw(const ShaderState& curSS) {
    // Enable the attributes used by our shader
    safe_glEnableVertexAttribArray(curSS.h_aPosition);
    safe_glEnableVertexAttribArray(curSS.h_aNormal);

    // bind vertex buffer object
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    safe_glVertexAttribPointer(curSS.h_aPosition, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNX), FIELD_OFFSET(VertexPNX, p));
    safe_glVertexAttribPointer(curSS.h_aNormal, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNX), FIELD_OFFSET(VertexPNX, n));

    // bind index buffer object
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

    // draw!
    glDrawElements(GL_TRIANGLES, iboLen, GL_UNSIGNED_SHORT, 0);

    // Disable the attributes used by our shader
    safe_glDisableVertexAttribArray(curSS.h_aPosition);
    safe_glDisableVertexAttribArray(curSS.h_aNormal);
  }
};

// Vertex buffer and index buffer associated with the different geometries
static shared_ptr<Geometry> g_cube, g_sphere_cpoint, g_octa, g_tube, g_sphere_moving, g_sphere_curve;

// --------- Scene

static const Cvec3 g_light1(2.0, 3.0, 14.0), g_light2(-2, -3.0, -5.0);  // define two light positions in world space
static Matrix4 g_eyeRbt = Matrix4::makeTranslation(Cvec3(0.0, 3.25, 10.0));
static const int g_numObjects = 27 + 5; // 3 slabs of 9 cubes each.
static Matrix4 g_objectRbt[g_numObjects] = {}; // each object gets its own RBT  

static Bezier curve = Bezier(Cvec3(1, 3, 0), Cvec3(2, 2, 2), Cvec3(2, 2, 4), Cvec3(3, 1, 5));

static void initBezierPoints(int index) {
  g_objectRbt[index] = Matrix4::makeTranslation(curve.p0);
  g_objectRbt[index + 1] = Matrix4::makeTranslation(curve.p1);
  g_objectRbt[index + 2] = Matrix4::makeTranslation(curve.p2);
  g_objectRbt[index + 3] = Matrix4::makeTranslation(curve.p3);
}


static void initTranslations(int y, int index) {
  for (int i = 0; i < 9; i++) {
    if (i <= 2) {
      g_objectRbt[i + index] = Matrix4::makeTranslation(Cvec3(-4, y, (i % 3)));
    }
    else if (i <= 5) {
      g_objectRbt[i + index] = Matrix4::makeTranslation(Cvec3(0, y, (i % 3)));
    }
    else {
      g_objectRbt[i + index] = Matrix4::makeTranslation(Cvec3(4, y, (i % 3)));
    }
  }
}

static void initMovingObject(int index) {
  g_objectRbt[index] = Matrix4::makeTranslation(curve.getPoint((double)-0.5));
}


///////////////// END OF G L O B A L S //////////////////////////////////////////////////

static void initObjects() {
  // each kind of geometry needs to be initialized here
  int ibLen, vbLen;
  getCubeVbIbLen(vbLen, ibLen);

  // Temporary storage for vertex and index buffers
  vector<VertexPNX> vtx(vbLen);
  vector<unsigned short> idx(ibLen);

  makeCube(0.5, vtx.begin(), idx.begin());
  g_cube.reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen));

  getSphereVbIbLen(30, 20, vbLen, ibLen);
  vtx.resize(vbLen);
  idx.resize(ibLen);
  makeSphere(0.5, 30, 20, vtx.begin(), idx.begin());
  g_sphere_moving.reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen));


  getSphereVbIbLen(30, 20, vbLen, ibLen);
  vtx.resize(vbLen);
  idx.resize(ibLen);
  makeSphere(0.25, 30, 20, vtx.begin(), idx.begin());
  g_sphere_cpoint.reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen));

  getSphereVbIbLen(30, 20, vbLen, ibLen);
  vtx.resize(vbLen);
  idx.resize(ibLen);
  makeSphere(0.10, 30, 20, vtx.begin(), idx.begin());
  g_sphere_curve.reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen));

  // TODO: add octahedron, tube

  getOctahedronVbIbLen(vbLen, ibLen);
  vtx.resize(vbLen);
  idx.resize(ibLen);
  makeOctahedron(0.5, vtx.begin(), idx.begin());
  g_octa.reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen));


  getTubeVbIbLen(36, vbLen, ibLen);
  vtx.resize(vbLen);
  idx.resize(ibLen);
  makeTube(1, 4, 36, vtx.begin(), idx.begin());
  g_tube.reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen));
  
}

// takes a projection matrix and send to the the shaders
static void sendProjectionMatrix(const ShaderState& SS, const Matrix4& projMatrix) {
  GLfloat glmatrix[16];
  projMatrix.writeToColumnMajorMatrix(glmatrix); // send projection matrix
  safe_glUniformMatrix4fv(SS.h_uProjMatrix, glmatrix);
}

// takes MVM and its normal matrix to the shaders
static void sendModelViewNormalMatrix(const ShaderState& SS, const Matrix4& MVM, const Matrix4& NMVM) {
  GLfloat glmatrix[16];
  MVM.writeToColumnMajorMatrix(glmatrix); // send MVM
  safe_glUniformMatrix4fv(SS.h_uModelViewMatrix, glmatrix);

  NMVM.writeToColumnMajorMatrix(glmatrix); // send NMVM
  safe_glUniformMatrix4fv(SS.h_uNormalMatrix, glmatrix);
}

// update g_frustFovY from g_frustMinFov, g_windowWidth, and g_windowHeight
static void updateFrustFovY() {
  if (g_windowWidth >= g_windowHeight)
    g_frustFovY = g_frustMinFov;
  else {
    const double RAD_PER_DEG = 0.5 * CS150_PI/180;
    g_frustFovY = atan2(sin(g_frustMinFov * RAD_PER_DEG) * g_windowHeight / g_windowWidth, cos(g_frustMinFov * RAD_PER_DEG)) / RAD_PER_DEG;
  }
}

static Matrix4 makeProjectionMatrix() {
  return Matrix4::makeProjection(
           g_frustFovY, g_windowWidth / static_cast <double> (g_windowHeight),
           g_frustNear, g_frustFar);
}

static void drawScene() {
  const Matrix4 projmat = makeProjectionMatrix(); // build projection matrix

  Matrix4 invEyeRbt = inv(g_eyeRbt);

  if (viewPoint == "m")
    invEyeRbt = inv(g_objectRbt[0]);
  
  const Cvec3 eyeLight1 = Cvec3(invEyeRbt * Cvec4(g_light1, 1)); // g_light1 position in eye coordinates
  const Cvec3 eyeLight2 = Cvec3(invEyeRbt * Cvec4(g_light2, 1)); // g_light2 position in eye coordinates

  // g_animIncrement is a small amount that is scaled to the framerate; the
  // following matrix will rotate through a small angle so that a total of 
  // 360 degrees is covered for every cycle of the clock parameter g_animClock
  // from 0 to 1.
  const Matrix4 rotatorY = Matrix4::makeYRotation(g_animIncrement*360); // rotate 360 per parameter cycle 0..1
  const Matrix4 rotatorX = Matrix4::makeXRotation(g_animIncrement*360);
  const Matrix4 rotatorZ = Matrix4::makeZRotation(g_animIncrement*360);

  const ShaderState& curSS = *g_shaderStates[g_activeShader]; // alias for currently selected shader

  glUseProgram(curSS.program); // select shader we want to use
  sendProjectionMatrix(curSS, projmat); // send projection matrix to shader
  safe_glUniform3f(curSS.h_uLight, eyeLight1[0], eyeLight1[1], eyeLight1[2]); // shaders need light positions
  safe_glUniform3f(curSS.h_uLight2, eyeLight2[0], eyeLight2[1], eyeLight2[2]);


  // draw the bezier curve itself using spheres
  int num_curve_spheres = 600;
  for (int i = 0; i < num_curve_spheres && drawCurve; ++i) {
    Matrix4 MVM = invEyeRbt * Matrix4::makeTranslation(curve.getPoint((float)i / (float)num_curve_spheres));
    Matrix4 NMVM = normalMatrix(MVM);
    sendModelViewNormalMatrix(curSS, MVM, NMVM);
    g_sphere_curve->draw(curSS);
  }


  for (int i = 0; i < g_numObjects; i++) {
    Matrix4 MVM = invEyeRbt * g_objectRbt[i];
    Matrix4 NMVM = normalMatrix(MVM);
    sendModelViewNormalMatrix(curSS, MVM, NMVM);

    if (g_objToManip == i) {
      safe_glUniform3f(curSS.h_uColor, 1.0, 0.0, 1.0); // use clock parameter to color object
    }
    else if (i >= 1 && i < 5) {
      safe_glUniform3f(curSS.h_uColor, 1.0, 1.0, 0.0); // use clock parameter to color object
    }
    else if (i == 0) {
      safe_glUniform3f(curSS.h_uColor, 1.0-g_animClock, 0.0, g_animClock); // use clock parameter to color object
    }
    else {
      safe_glUniform3f(curSS.h_uColor, (i % 2), (i + 1) % 2, (i + 1) % 3); // use clock parameter to color object
    }

    if (i == 0) {
      g_objectRbt[0] = Matrix4::makeTranslation(curve.getPoint(1 - g_animClock));
      if (viewPoint == "s") {
	g_sphere_moving->draw(curSS);
      }
    }
    if (i < 5 && i >= 1) {
      g_sphere_cpoint->draw(curSS);
    }
    if (i >= 5 && i < g_numObjects ) {
      g_octa->draw(curSS);
    }
  }
  // TODO: Remove cube. Add octahedron, tube, and sphere to scene and make them chase each other.
}

static void display() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);   // clear framebuffer color&depth
  drawScene();
  glutSwapBuffers();                                    // show the back buffer (where we rendered stuff)
  checkGlErrors();

  // Calculate frames per second 
  static int oldTime = -1;
  static int frames = 0;
  static int lastTime = -1;

  int currentTime = glutGet(GLUT_ELAPSED_TIME); // returns milliseconds
  g_elapsedTime = currentTime - lastTime;       // how long last frame took
  lastTime = currentTime;

        if (oldTime < 0)
                oldTime = currentTime;

        frames++;

        if (currentTime - oldTime >= 5000) // report FPS every 5 seconds
        {
                cout << "Frames per second: "
                        << float(frames)*1000.0/(currentTime - oldTime) << endl;
                cout << "Elapsed ms since last frame: " << g_elapsedTime << endl;
                oldTime = currentTime;
                frames = 0;
        }
}

static void reshape(const int w, const int h) {
  g_windowWidth = w;
  g_windowHeight = h;
  glViewport(0, 0, w, h);
  updateFrustFovY();
  glutPostRedisplay();
}

static void motion(const int x, const int y) {
  const double dx = x - g_mouseClickX;
  const double dy = g_windowHeight - y - 1 - g_mouseClickY;

  Matrix4 m, a;
  if (g_mouseLClickButton && !g_mouseRClickButton) { // left button down?
    m = Matrix4::makeXRotation(-dy) * Matrix4::makeYRotation(dx);
  }
  else if (g_mouseRClickButton && !g_mouseLClickButton) { // right button down?
    m = Matrix4::makeTranslation(Cvec3(dx, dy, 0) * 0.01);
  }
  else if (g_mouseMClickButton || (g_mouseLClickButton && g_mouseRClickButton)) {  // middle or (left and right) button down?
    m = Matrix4::makeTranslation(Cvec3(0, 0, -dy) * 0.01);
  }

  if (g_mouseClickDown) {
	  a =  transFact(g_objectRbt[g_objToManip])*linFact(g_eyeRbt);
	  g_objectRbt[g_objToManip] = a * m * inv(a) * g_objectRbt[g_objToManip];
	  if (g_objToManip <= 4 && g_objToManip > 0) {
	    if (g_objToManip == 1) {
	      curve.p0 = Cvec3(g_objectRbt[g_objToManip](0, 3), g_objectRbt[g_objToManip](1, 3), g_objectRbt[g_objToManip](2, 3));
	    }
	    else if (g_objToManip == 2) {
	      curve.p1 = Cvec3(g_objectRbt[g_objToManip](0, 3), g_objectRbt[g_objToManip](1, 3), g_objectRbt[g_objToManip](2, 3));
	    }
	    else if (g_objToManip == 3) {
	      curve.p2 = Cvec3(g_objectRbt[g_objToManip](0, 3), g_objectRbt[g_objToManip](1, 3), g_objectRbt[g_objToManip](2, 3));
	    }
	    else if (g_objToManip == 4) {
	      curve.p3 = Cvec3(g_objectRbt[g_objToManip](0, 3), g_objectRbt[g_objToManip](1, 3), g_objectRbt[g_objToManip](2, 3));
	    }
	  }
	  glutPostRedisplay(); // we always redraw if we changed the scene
  }

  g_mouseClickX = x;
  g_mouseClickY = g_windowHeight - y - 1;
}

static void mouse(const int button, const int state, const int x, const int y) {
  g_mouseClickX = x;
  g_mouseClickY = g_windowHeight - y - 1;  // conversion from GLUT window-coordinate-system to OpenGL window-coordinate-system

  g_mouseLClickButton |= (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN);
  g_mouseRClickButton |= (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN);
  g_mouseMClickButton |= (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN);

  g_mouseLClickButton &= !(button == GLUT_LEFT_BUTTON && state == GLUT_UP);
  g_mouseRClickButton &= !(button == GLUT_RIGHT_BUTTON && state == GLUT_UP);
  g_mouseMClickButton &= !(button == GLUT_MIDDLE_BUTTON && state == GLUT_UP);

  g_mouseClickDown = g_mouseLClickButton || g_mouseRClickButton || g_mouseMClickButton;
}

static void idle()
{
  g_animIncrement = g_animSpeed * g_elapsedTime / 1000; // rescale animation increment
  g_animClock += g_animIncrement;           // Update animation clock 
         if (g_animClock > g_animMax)       // and cycle to start if necessary.
		 g_animClock = g_animStart;
         glutPostRedisplay();  // for animation
}

static void keyboard(const unsigned char key, const int x, const int y) {
  switch (key) {
  case 27:
    exit(0);                                  // ESC
  case 'h':
    cout << " ============== H E L P ==============\n\n"
    << "h\t\thelp menu\n"
    << "s\t\tsave screenshot\n"
    << "o\t\tCycle object to manipulate\n"
    << "f\t\tCycle fragment shader\n"
    << "f\t\tCycle viewpoint\n"
    << "+\t\tIncrease animation speed\n"
    << "-\t\tDecrease animation speed\n"
    << "drag left mouse to rotate\n" 
    << "drag middle mouse to translate in/out \n" 
    << "drag right mouse to translate up/down/left/right\n" 
    << endl;
    break;
  case 's':
    glFlush();
    writePpmScreenshot(g_windowWidth, g_windowHeight, "out.ppm");
    cout << "Screenshot written to out.ppm." << endl;
    break;
  case 'o':
    g_objToManip = (g_objToManip + 1) % 5;
    break;
  case '+':
    g_animSpeed *= 1.05;
    break;
  case '-':
    g_animSpeed *= 0.95;
    break;
  case 'f':
    break;
  case 'p':
    drawCurve = !drawCurve;
    break;
  case 'v':
    if (viewPoint == "m") {
      viewPoint = "s";
      break;
    }
    else if (viewPoint == "s") {
      viewPoint = "m";
      break;
    }
    break;
  }
  glutPostRedisplay();
}

static void initGlutState(int argc, char * argv[]) {
  glutInit(&argc, argv);                                  // initialize Glut based on cmd-line args
  glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);  //  RGBA pixel channels and double buffering
  glutInitWindowSize(g_windowWidth, g_windowHeight);      // create a window
  glutCreateWindow("Project 4: Equilibrium");    // title the window

  glutDisplayFunc(display);                               // display rendering callback
  glutReshapeFunc(reshape);                               // window reshape callback
  glutMotionFunc(motion);                                 // mouse movement callback
  glutMouseFunc(mouse);                                   // mouse click callback
  glutIdleFunc(idle);  					  // idle callback for animation
  glutKeyboardFunc(keyboard);
}

static void initGLState() {
  glClearColor(0, 0, 0, 0.);
  glClearDepth(0.);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glCullFace(GL_BACK);
//  glEnable(GL_CULL_FACE); // Enable if you don't want to render back faces,
                            // but make sure it's disabled to show inside of tube.
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GREATER);
  glReadBuffer(GL_BACK);
  glEnable(GL_BLEND); // Enable alpha blending
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (!g_Gl2Compatible)
    glEnable(GL_FRAMEBUFFER_SRGB);
}

static void initShaders() {
  g_shaderStates.resize(g_numShaders);
  for (int i = 0; i < g_numShaders; ++i) {
    if (g_Gl2Compatible)
      g_shaderStates[i].reset(new ShaderState(g_shaderFilesGl2[i][0], g_shaderFilesGl2[i][1]));
    else
      g_shaderStates[i].reset(new ShaderState(g_shaderFiles[i][0], g_shaderFiles[i][1]));
  }
}

static void initGeometry() {
  initObjects();
}

int main(int argc, char * argv[]) {
  try {
    initGlutState(argc,argv);

    glewInit(); // load the OpenGL extensions

    cout << (g_Gl2Compatible ? "Will use OpenGL 2.x / GLSL 1.0" : "Will use OpenGL 3.x / GLSL 1.3") << endl;
    if ((!g_Gl2Compatible) && !GLEW_VERSION_3_0)
      throw runtime_error("Error: card/driver does not support OpenGL Shading Language v1.3");
    else if (g_Gl2Compatible && !GLEW_VERSION_2_0)
      throw runtime_error("Error: card/driver does not support OpenGL Shading Language v1.0");

    initMovingObject(0);
    initBezierPoints(1);
    initTranslations(1, 5);
    initTranslations(3, 14);
    initTranslations(5, 23);


    initGLState();
    initShaders();
    initGeometry();

    glutMainLoop();
    return 0;
  }
  catch (const runtime_error& e) {
    cout << "Exception caught: " << e.what() << endl;
    return -1;
  }
}
