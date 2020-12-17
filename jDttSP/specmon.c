/* specmon.c */

#include <common.h>

#define SLEEP (500000)

char *cmdsink = "./IPC/SDR-1000-0-commands.fifo",
     *specsrc = "./IPC/SDR-1000-0-spec.fifo";

FILE *cmdfp, *spcfp;

int label;
float spec[DEFSPEC];

int
main(int argc, char **argv) {
  int i = 0, j, k, lab = getpid();

  if (!(cmdfp = fopen(cmdsink, "r+")))
    perror(cmdsink), exit(1);
  if (!(spcfp = fopen(specsrc, "r+")))
    perror(specsrc), exit(1);

  printf("spec OK\n");

  for (;;) {

    usleep(SLEEP);

    fprintf(cmdfp, "reqSpectrum %d\n", lab);
    fflush(cmdfp);

    if (fread((char *) &label, sizeof(int), 1, spcfp) != 1)
      perror("fread spectrum label"), exit(1);

    if (fread((char *) spec, sizeof(float), DEFSPEC, spcfp) != DEFSPEC)
      perror("fread spec"), exit(1);

    printf("%d <%d>", i++, label);

    j = 0;
    for (k = 1; k < DEFSPEC; k++)
      if (spec[k] > spec[j]) j = k;
    printf(" [%d %g]\n", j, spec[j]);
  }

  fclose(cmdfp);
  fclose(spcfp);

  exit(0);
}
