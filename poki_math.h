/*
  @file poki_math.h
  poki~
 
  created 11/16/10 emb
*/

// #define Abs(x)    ((x) < 0 ? -(x) : (x))
// #define Max(a, b) ((a) > (b) ? (a) : (b))

//inline double abs(double x) { return ((x < 0.0) ? -1.0 * x : x); }

/*
double RelDif(double a, double b)
{
	double c = a < 0.0 ? -1.0 * a : a;
	double d = b < 0.0 ? -1.0 * b : b;
  
	d = (d > c ? d : c);
  const double a_b = a-b;
  
	return d == 0.0 ? 0.0 : (a_b < 0.0 ? -1.0 * a_b : a_b) / d;
}
 */
inline double absDif(double a, double b)
{
  const double dif = a-b;
  return dif < 0.0 ? -1.0 * dif : dif;
}

inline static float hermite_interp(const double x,
                                   const double y0, 
                                   const double y1, 
                                   const double y2, 
                                   const double y3)
{
	const double c0 = y1;
	const double c1 = 0.5f * (y2 - y0);
	const double c3 = 1.5f * (y1 - y2) + 0.5f * (y3 - y0);
	const double c2 = y0 - y1 + c1 - c3;
	return (float)(((c3 * x + c2) * x + c1) * x + c0);
}


