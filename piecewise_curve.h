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

};

