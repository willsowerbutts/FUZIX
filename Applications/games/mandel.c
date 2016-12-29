/* WRS: 
 * From http://freaknet.org/martin/tape/gos/src/mandel.c
 * Martin Guy, <martinwguy@yahoo.it>
 */

/*
 *	MANDEL.C
 *
 * Compute mandelbrot fractals and display the results on a textual
 * device as characters of increasing darkness.
 *
 * The output is a 2-dimensional picture, which represents a part
 * of the complex plane.  The value at each point is calculated as follows:
 * 
 * Let C be the complex coordinates of the point whose value we want to know.
 * Set Z = C and then iterate Z = Z^2 + C.
 *
 * One of two things will happen: either Z will go zooming off to infinity
 * or it will remain near the origin.  If Z never goes to infinity,
 * the value of C is within the Mandelbrot set.
 *
 * It can been shown that if the modulus of Z exceeds 2.0, it will
 * definitely go off to infinity, so we stop when this happens, and the
 * value of the function at point C is the number of iterations it took.
 *
 * To prevent infinite looping on points which are inside the Mandelbrot set,
 * we impose a maximum iteration count, MAXITER.  If we iterate this many times
 * and are still within the boundary, the return value is MAXITER.
 *
 *	Martin Guy, UKC, February 1989.
 */

/* shut up sdcc warnings */
// #define double float

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* constants */
#define SCR_SIZE_X	80	/* number of pixels across the screen */
#define SCR_SIZE_Y	39	/* number of pixels down the screen */

/* Maximum number of iterations before we assume that the point is
 * inside the mandelbrot set */
#define MAXITER 14

/* 
 * iterate - perform the mandelbrot iteration from a given starting
 * point, returning the number of iterations performed before |z| > 2,
 * which will be in the range 0 .. MAXITER-1.
 * If the starting value seems to be in the mandelbrot set, it returns MAXITER.
 */

int iterate(float cr, float ci)
{
	int niter;	/* Number of iterations we have done. */
	float zr, zi;	/* Iteration variable, real and imaginary. */
	float zr2, zi2;/* Intermediate results of zr^2 and zi^2. */

	zr = cr; zi = ci;

	for (niter = 0; niter < MAXITER; niter++) {

		/* Cache zr^2 and zi^2 'cos they're needed twice. */

		zr2 = zr * zr;
		zi2 = zi * zi;

		/* If modulus of z is > 2, it's not in the set. */

		if (zr2 + zi2 > 4.0) break;

		/* z = z^2 + c
		 * The order of these two statements is important because
		 * otherwise "zi = ..." would get the new value of zr!
		 */
		zi = 2 * zr * zi + ci;
		zr = zr2 - zi2 + cr;
	}

	return(niter);
}

/* Convert iteration count to the character to display for that count. */

//                01234567890123
char codetab[] = " .,:;|I*#ABCDEFGHIJK";
//#define NCODES MAXITER

char iter_to_char(int value)
{
	/* Points inside the set are distinct. */
	if (value == MAXITER) return(' ');

	/* Scale value into number of colours we have.
	 * Input values: 0..MAXITER-1
	 * Output values: 0..NCODES-1
	 * The following equation maps the same number of input values
	 * to each output value (+/- 1).
	 */
	//value = (value * NCODES) / MAXITER;

	/* and return the appropriate character */
	return(codetab[value]);
}

void iter_to_color(int value)
{
    if(value == MAXITER) 
        printf("\x1b[0m");
    else if(value < 7){ // 0 ... 7
        printf("\x1b[0;%dm", 31+value); // 31 ... 37
    }else{          // 7 ... 13
        printf("\x1b[1;%dm", 24+value); // 31 ... 37 bright
    }
}

void reset_tty(int a)
{
    printf("\x1b[0m");
    a;
}

void main(int argc, char **argv)
{
	int x, y;	/* Loop variables used to scan output image. */
			/* Values are 0 .. MAX-1, (0,0) is top left. */

	float cr, ci;		/* x & y mapped onto complex plane */
	float step_r, step_i;	/* size of one pixel in complex */
    float min_r = -2.0;
    float min_i = -2.0;
    float max_r =  2.0;
    float max_i =  2.0;
	int value; 

    if(argc > 4){ /* try -2 -1.5 0.75 1.5 */
        min_r = atof(argv[1]);
        min_i = atof(argv[2]);
        max_r = atof(argv[3]);
        max_i = atof(argv[4]);
    }

    argc; argv;
    // atexit(reset_tty);

	/* precompute loop steps */
	step_r = (max_r - min_r) / SCR_SIZE_X;
	step_i = (max_i - min_i) / SCR_SIZE_Y;

	/* We start at the top, so ci starts at max_i and counts down */
	for (y = 0, ci = max_i;  y < SCR_SIZE_Y;  y++, ci -= step_i) {

		for (x = 0, cr = min_r;  x < SCR_SIZE_X;  x++, cr += step_r) {
			value = iterate(cr, ci);
            iter_to_color(value);
            putchar(iter_to_char(value));
            fflush(stdout);
		}
		putchar('\n');
	}

    reset_tty(0);
}

