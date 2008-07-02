#include <gsl/gsl_sf.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char *argv[]) {
  if(argc != 4) {
    fprintf(stderr, "Wrong number of arguments: %d\n", argc);
    exit(1);
  }

  long int meanN = strtol(argv[1], NULL, 10);

  double d = strtod(argv[2], NULL);
  double sigma2 = strtod(argv[3], NULL);

  double logw = (2 / d) * gsl_sf_log(-gsl_sf_log(0.99));
  double logxthresh = gsl_sf_log(sigma2) + logw
    - (2 / d) * gsl_sf_log(meanN)
    - gsl_sf_log(d/2)
    - (2 / d) * gsl_sf_log(2 / d)
    + (2 / d) * gsl_sf_lngamma(d / 2);

  printf("w: %f\n", exp(logw));
  printf("x_thresh: %f\n", exp(logxthresh));
  exit(0);
}
