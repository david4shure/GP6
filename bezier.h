#include "cvec.h"
#include <math.h>

class Bezier {
public:
  Cvec3 p0, p1, p2, p3;
  bool isVisible;
  Bezier() {
    isVisible = true;
    p0 = Cvec3(0.0, 0.0, 0.0);
    p1 = Cvec3(1.0, 1.0, 1.0);
    p2 = Cvec3(2.0, 2.0, 2.0);
    p3 = Cvec3(3.0, 3.0, 3.0);
 }
  Bezier(Cvec3 g_p0, Cvec3 g_p1, Cvec3 g_p2, Cvec3 g_p3) { 
    isVisible = true;
    p0 = g_p0;
    p1 = g_p1;
    p2 = g_p2;
    p3 = g_p3;
  }

  Cvec3 getPoint(float t) {

    Cvec3 cop_p0 = p0;
    Cvec3 cop_p1 = p1;
    Cvec3 cop_p2 = p2;
    Cvec3 cop_p3 = p3;

    double p0_coef = powf(1 - t, 3);
    double p1_coef = 3 * t * powf(1 - t, 2);
    double p2_coef = 3 * powf(t, 2) * (1 - t);
    double p3_coef = powf(t, 3);

    cop_p0 *= p0_coef;
    cop_p1 *= p1_coef;
    cop_p2 *= p2_coef;
    cop_p3 *= p3_coef;

    return cop_p0 + cop_p1 + cop_p2 + cop_p3;
  }

  Cvec3 getDerivative(float t) {

    Cvec3 cop_p0 = p0;
    Cvec3 cop_p1 = p1;
    Cvec3 cop_p2 = p2;
    Cvec3 cop_p3 = p3;

    double p0_coef = 3 * powf(1 - t, 2);
    double p1_coef = 3 - 12 * t +  9 * powf(t, 2);
    double p2_coef = 6 * t - 9 * powf(t, 2);
    double p3_coef = powf(t, 3);

    cop_p0 *= p0_coef;
    cop_p1 *= p1_coef;
    cop_p2 *= p2_coef;
    cop_p3 *= p3_coef;

    return cop_p0 + cop_p1 + cop_p2 + cop_p3;
  }

};
