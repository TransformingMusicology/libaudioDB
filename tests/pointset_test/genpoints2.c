#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

double randn();
double randbl();

/* genpoints count radius^2 */
int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s count radius^2 [dim]\n", argv[0]);
    exit(1);
  }
  long int count = strtol(argv[1], NULL, 0);
  double rsquared = strtod(argv[2], NULL);
  long int dim = 3;
  if(argc > 3)
    dim = strtol(argv[3], NULL, 0);
  
  // Generate *count* Gaussian Random vectors in R^*dim*
  // sitting on the *rdashed*-sphere

  srandom(time(NULL));

  int i,j;
  for (i = 0; i < count + 1; i++) {
    // Normed Gaussian random vectors are distributed uniformly on unit sphere
    double* coords = malloc(dim * sizeof(double));
    double nmsq = 0.0;

    for (j = 0; j < dim; j++){
      if(i < count)
	coords[j] = randn();
      else
	coords[j] = 0.0;
      nmsq += coords[j]*coords[j];
    }

    double nm2 = 0.0;
    if(i < count){
      nm2 = sqrt(rsquared/nmsq);
      // Place on rdash-sphere
      for (j = 0; j < dim; j++)
	coords[j] *= nm2;
    }
    // Translate to (0,0,...,1)
    coords[dim-1]+=1.0; 

    // Compute distance to (0,0,...,1)
    nmsq = 0.0;
    for (j = 0; j < dim-1; j++){
      nmsq += coords[j]*coords[j];
    }
    // Save last value to distance calulcation to query(0,0,...,1)
    double nth = coords[dim-1];
    // Output to ASCII terminal
    printf("(");
    for(j = 0; j < dim; j++)
      printf("%8.3f ", coords[j]);
    printf(") d = %8.3f\n", sqrt(nmsq + (nth-1)*(nth-1)));
    

    // Save single feature vector
    char name[40];
    if(i < count)
      snprintf(name, 39, i<10?"testfeature0%d":"testfeature%d", i);
    else
      snprintf(name, 39, "queryfeature");
    /* assumes $PWD is right */
    int fd = open(name, O_CREAT|O_TRUNC|O_WRONLY, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);

    write(fd, &dim, sizeof(int));
    for(j = 0; j < dim; j++)
      write(fd, coords + j, sizeof(double));
    close(fd);

    free(coords);
  }
  exit(0);
}

// Genereate U[0,1]
double randbl(){
  return (   (double)rand() / ((double)(RAND_MAX)+(double)(1)) );
}

// Generate z ~ N(0,1)
double randn(){
// Box-Muller
  double x1, x2;
  do{
    x1 = randbl();
  } while (x1 == 0); // cannot take log of 0
  x2 = randbl();
  double z = sqrt(-2.0 * log(x1)) * cos(2.0 * M_PI * x2);
  return z;
}

