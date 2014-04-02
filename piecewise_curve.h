#include "cvec.h"
#include <math.h>
#include <stdio.h>

using namespace std;

class PiecewiseCurve {
 public:
  Bezier c1, c2, c3, c4, c5;
  
  PiecewiseCurve() {
  }

  PiecewiseCurve(Bezier& g_c1, Bezier& g_c2, Bezier& g_c3, Bezier& g_c4, Bezier& g_c5) {
    c1 = g_c1;
    c2 = g_c2;
    c3 = g_c3;
    c4 = g_c4;
    c5 = g_c5;
  }


  // The entire curve is parametrized from t = 0 - 1, so we need to be somewhat clever
  // 0 - 0.2 -> c1
  // 0.2 - 0.4 -> c2
  // 0.4 - 0.6 -> c3
  // 0.6 - 0.8 -> c4
  // 0.8 - 1.0 -> c5
  Cvec3 getPoint(float t) {

    if (t >= 0 && t < 0.2) {
      return c1.getPoint(5 * t);
    }
    else if (t >= 0.2 && t < 0.4) {
      return c2.getPoint(5 * t - 1);
    }
    else if (t >= 0.4 && t < 0.6) {
      return c3.getPoint(5 * t - 2);
    }
    else if (t >= 0.6 && t < 0.8) {
      return c4.getPoint(5 * t - 3);
    }
    else if (t >= 0.8 && t <= 1.0) {
      return c5.getPoint(5 * t - 4);
    }
    else {
      return c1.getPoint(t);
    }
  }

  Cvec3 cross(Cvec3 u, Cvec3 z) {
    return Cvec3(((u[1] * z[2]) - (u[2] * z[1])), ((u[2] * z[0]) - (u[0] * z[2])), ((u[0] * z[1]) - (u[1] * z[0])));
  }

  Matrix4 lookAheadFrom(Cvec3 position, float t) {
    float epsilon = 0.002;
    Cvec3 p = position;
    Cvec3 q = this->getPoint(t + epsilon);
    Cvec3 var0 = (p - q).normalize();
    Cvec3 z = var0;
    Cvec3 u = Cvec3(0, 1, 0);
    Cvec3 var = cross(u, z).normalize();
    Cvec3 x = var;
    Cvec3 y = cross(z, x);

    Matrix4 matrixRep = Matrix4();
    
    matrixRep(0, 0) = x[0];
    matrixRep(0, 1) = y[0];
    matrixRep(0, 2) = z[0];
    matrixRep(0, 3) = p[0];
    
    matrixRep(1, 0) = x[1];
    matrixRep(1, 1) = y[1];
    matrixRep(1, 2) = z[1];
    matrixRep(1, 3) = p[1];
    
    matrixRep(2, 0) = x[2];
    matrixRep(2, 1) = y[2];
    matrixRep(2, 2) = z[2];
    matrixRep(2, 3) = p[2];

    matrixRep(3, 0) = 0;
    matrixRep(3, 1) = 0;
    matrixRep(3, 2) = 0;
    matrixRep(3, 3) = 1;
    
    return matrixRep;
  }

};

