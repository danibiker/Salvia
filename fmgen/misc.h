#ifndef MISC_H
#define MISC_H

#define FMGEN_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define FMGEN_MIN(x, y) (((x) < (y)) ? (x) : (y))

inline int Limit(int v, int max, int min) 
{ 
	return v > max ? max : (v < min ? min : v); 
}

#endif /* MISC_H */

