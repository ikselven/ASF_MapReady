#include "asf.h"
#include "asf_meta.h"
#include <unistd.h>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_multiroots.h>

#define NUM_COEFS 25

double value_at_pixel25(double inval1, double inval2, double *coef)
{
  double temp;
  temp = coef[ 0]*inval1*inval1*inval1*inval1*inval2*inval2*inval2*inval2 +
         coef[ 1]*inval1*inval1*inval1*       inval2*inval2*inval2*inval2 +
         coef[ 2]*inval1*inval1*              inval2*inval2*inval2*inval2 +
         coef[ 3]*inval1*                     inval2*inval2*inval2*inval2 +
         coef[ 4]*                            inval2*inval2*inval2*inval2 +
         coef[ 5]*inval1*inval1*inval1*inval1*inval2*inval2*inval2 +
         coef[ 6]*inval1*inval1*inval1*       inval2*inval2*inval2 +
         coef[ 7]*inval1*inval1*              inval2*inval2*inval2 +
         coef[ 8]*inval1*                     inval2*inval2*inval2 +
         coef[ 9]*                            inval2*inval2*inval2 +
         coef[10]*inval1*inval1*inval1*inval1*inval2*inval2 +
         coef[11]*inval1*inval1*inval1*       inval2*inval2 +
         coef[12]*inval1*inval1*              inval2*inval2 +
         coef[13]*inval1*                     inval2*inval2 +
         coef[14]*                            inval2*inval2 +
         coef[15]*inval1*inval1*inval1*inval1*inval2 +
         coef[16]*inval1*inval1*inval1*       inval2 +
         coef[17]*inval1*inval1*              inval2 +
         coef[18]*inval1*                     inval2 +
         coef[19]*                            inval2 +
         coef[20]*inval1*inval1*inval1*inval1 +
         coef[21]*inval1*inval1*inval1*       +
         coef[22]*inval1*inval1*              +
         coef[23]*inval1*                     +
         coef[24];
  return(temp);
}

double value_at_pixel9(double inval1, double inval2, double *coef)
{
  double temp;
  temp = 
         coef[0]*inval1*inval1*inval2*inval2 +
         coef[1]*inval1*       inval2*inval2 +
         coef[2]*              inval2*inval2 +
         coef[3]*inval1*inval1*inval2 +
         coef[4]*inval1*       inval2 +
         coef[5]*              inval2 +
         coef[6]*inval1*inval1 +
         coef[7]*inval1        +
         coef[8];
  return(temp);
}

struct find_offset_params {
        double x_start, y_start;	/* origin points    */
        double *inval1;		 	/* input x values   */
	double *inval2;			/* input y values   */
	double *outval;			/* output values    */
   	double cnt;			/* number of values */
					/* here we want outval1 = f(inval1,inval2) */
};

static double err_at_pixel(struct find_offset_params *p, double *c, double val, double x, double y)
{
    double xs, ys, xpt, ypt, myval, diff;

    xs = p->x_start;
    ys = p->y_start;

    xpt = x - xs;
    ypt = y - ys;

    myval = value_at_pixel25(xpt,ypt,c);

    diff = (myval-val);
    return(diff);
}


static int
getObjective(const gsl_vector *x, void *params, gsl_vector *f)
{
    double c[NUM_COEFS];
    int i;
    for (i=0; i<NUM_COEFS; i++) { c[i] = gsl_vector_get(x,i); }

    struct find_offset_params *p = (struct find_offset_params *)params;
    double *in1 = p->inval1;
    double *in2 = p->inval2;
    double *out = p->outval;
    double cnt  = p->cnt;

    double err = 0.0;

    for (i=0; i<cnt; i++) {
      err += err_at_pixel(p,c,out[i],in1[i],in2[i]);
    }
    for (i=0; i<NUM_COEFS; i++) { gsl_vector_set(f,i,err); }

    /*
    for (i=0; i<NUM_COEFS; i++) {
      err = err_at_pixel(p,c,out[i],in1[i],in2[i]);
      gsl_vector_set(f,i,err);
    }
    */


    return GSL_SUCCESS;
}


static void print_state(int iter, gsl_multiroot_fsolver *s)
{
    int i;

    printf("%3d ",iter);
    for (i=0; i<NUM_COEFS;i++)
      printf("%.3f ",gsl_vector_get(s->x,i));
    printf(" f(x) = %.8f\n", gsl_vector_get(s->f, 0));
}

/*
static void coarse_search(double t_extent_min, double t_extent_max,
                          double x_extent_min, double x_extent_max,
                          double *t_min, double *x_min,
                          struct refine_offset_params *params)
{
    double the_min = 9999999;
    double min_t=99, min_x=99;
    int i,j,k=6;
    double t_extent = t_extent_max - t_extent_min;
    double x_extent = x_extent_max - x_extent_min;
    gsl_vector *v = gsl_vector_alloc(2);
    gsl_vector *u = gsl_vector_alloc(2);
    //printf("           ");
    //for (j = 0; j <= k; ++j) {
    //    double x = x_extent_min + ((double)j)/k*x_extent;
    //    printf("%9.3f ", x);
    //}
    //printf("\n           ");
    //for (j = 0; j <= k; ++j)
    //    printf("--------- ");
    //printf("\n");
    for (i = 0; i <= k; ++i) {
        double t = t_extent_min + ((double)i)/k*t_extent;
        //printf("%9.3f | ", t);

        for (j = 0; j <= k; ++j) {
            double x = x_extent_min + ((double)j)/k*x_extent;

            gsl_vector_set(v, 0, t);
            gsl_vector_set(v, 1, x);
            getObjective(v,(void*)params, u);
            double n = gsl_vector_get(u,0);
            //printf("%9.3f ", n);
            if (n<the_min) { 
                the_min=n;
                min_t=gsl_vector_get(v,0);
                min_x=gsl_vector_get(v,1);
            }
        }
        //printf("\n");
    }

    *t_min = min_t;
    *x_min = min_x;

    gsl_vector_free(v);
    gsl_vector_free(u);
}
*/
/*
static void generate_start(struct refine_offset_params *params,
                           double *start_t, double *start_x)
{
    int i;

    double extent_t_min = -10;
    double extent_t_max = 10;

    double extent_x_min = -5000;
    double extent_x_max = 5000;

    double t_range = extent_t_max - extent_t_min;
    double x_range = extent_x_max - extent_x_min;

    for (i = 0; i < 12; ++i)
    {
        coarse_search(extent_t_min, extent_t_max, extent_x_min, extent_x_max,
                      start_t, start_x, params);

        t_range /= 3;
        x_range /= 3;

        extent_t_min = *start_t - t_range/2;
        extent_t_max = *start_t + t_range/2;

        extent_x_min = *start_x - x_range/2;
        extent_x_max = *start_x + x_range/2;

        //printf("refining search to region: (%9.3f,%9.3f)\n"
        //       "                           (%9.3f,%9.3f)\n",
        //       extent_t_min, extent_t_max,
        //       extent_x_min, extent_x_max);
    }

}
*/

static void generate_start(struct find_offset_params *params, double *c, double vals)
{
    int i;
    //for (i=0;i<NUM_COEFS;i++) c[i] = 0.01;
    c[NUM_COEFS-1] = vals;
}


void create_mapping(double xs, double ys, double valstart,  double *xin, double *yin, double *vals, int cnt, double *coefs)
{
    int status;
    int iter = 0, max_iter = 1000;
    int i;
    const gsl_multiroot_fsolver_type *T;
    gsl_multiroot_fsolver *s;
    gsl_error_handler_t *prev;
    const size_t n = NUM_COEFS;
    struct find_offset_params params;

    params.x_start= xs;
    params.y_start= ys;
    params.inval1 = xin;
    params.inval2 = yin;
    params.outval = vals;
    params.cnt = cnt;

    gsl_multiroot_function F = {&getObjective, n, &params};
    gsl_vector *x = gsl_vector_alloc(n);

    double c[NUM_COEFS];
    generate_start(&params, &c, valstart);
    for (i=0; i<NUM_COEFS; i++) gsl_vector_set (x, i, c[i]);

    T = gsl_multiroot_fsolver_hybrid;
    s = gsl_multiroot_fsolver_alloc(T, n);
    gsl_multiroot_fsolver_set(s, &F, x);

    prev = gsl_set_error_handler_off();

    do {
        ++iter;
        status = gsl_multiroot_fsolver_iterate(s);

        print_state(iter, s);

        // abort if stuck
        if (status) break;

        status = gsl_multiroot_test_residual (s->f, 1e-6);
    } while (status == GSL_CONTINUE && iter < max_iter);

    for (i=0; i<NUM_COEFS; i++) coefs[i] = gsl_vector_get(s->x,i);

    gsl_vector *retrofit = gsl_vector_alloc(n);
    for (i=0; i<NUM_COEFS; i++) gsl_vector_set(retrofit,i,coefs[i]);
    gsl_vector *output = gsl_vector_alloc(n);
    getObjective(retrofit, (void*)&params, output);
    double val= gsl_vector_get(output,0);
    printf("GSL Result: %f \n",val);
    gsl_vector_free(retrofit);
    gsl_vector_free(output);

    gsl_multiroot_fsolver_free(s);
    gsl_vector_free(x);
    gsl_set_error_handler(prev);

}