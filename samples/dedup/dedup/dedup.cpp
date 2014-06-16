#include <tbb/tick_count.h>
#include <iostream>

#include "util.h"
#include "dedup_debug.h"
#include "dedupdef.h"
#include "encoder.h"
#include "decoder.h"
#include "hashtable.h"
#include "config.h"
#include "queue.h"

#include <getopt.h>

#ifdef PARALLEL
#include <pthread.h>
#endif //PARALLEL

#ifdef ENABLE_PARSEC_HOOKS
#include <hooks.h>
#endif

config * conf;

static int
keys_equal_fn ( void *key1, void *key2 ){
  return (memcmp(((CacheKey*)key1)->sha1name, ((CacheKey*)key2)->sha1name, SHA1_LEN) == 0);
}



/*--------------------------------------------------------------------------*/
static void
usage(char* prog)
{
  //printf("usage: %s [-cusfh] [-w gzip/bzip2/none] [-i file/hostname] [-o file/hostname] [-b block_size_in_KB] [-t number_of_threads]\n",prog);
  printf("usage: %s [-c] [-i file] [-o file]\n",prog);
  printf("-c \t\t\tcompress\n");
  //printf("-s \t\t\tsend output to the receiver\n");
  //printf("-f \t\t\tput output into a file\n");
  //printf("-w \t\t\tcompress/uncompress technique: gzip/bzip2/none\n");
  printf("-i file\t\t\tthe input file/src host\n");
  printf("-o file\t\t\tthe output file/dst host\n");
  //printf("-t number of threads per stage \n");
  printf("-h \t\t\thelp\n");
}
/*--------------------------------------------------------------------------*/
int 
main(int argc, char** argv)
{
  tbb::tick_count t0 = tbb::tick_count::now();

#ifdef PARSEC_VERSION
#define __PARSEC_STRING(x) #x
#define __PARSEC_XSTRING(x) __PARSEC_STRING(x)
  printf("PARSEC Benchmark Suite Version "__PARSEC_XSTRING(PARSEC_VERSION)"\n");
#else
  printf("PARSEC Benchmark Suite\n");
#endif //PARSEC_VERSION
#ifdef ENABLE_PARSEC_HOOKS
        __parsec_bench_begin(__parsec_dedup);
#endif

  int32 compress = TRUE;
  
  conf = (config *) malloc(sizeof(config)); 
  memset(conf,0,sizeof(config));
  conf->b_size = 32 * 1024;
  compress_way = COMPRESS_GZIP;
  strcpy(conf->outfile, "");
  conf->preloading = 0;
  conf->nthreads = 8;
  conf->method = METHOD_FILE;

  //parse the args

  if (argc <= 1) {
      usage(argv[0]);
      return -1;
  }
  
  char ch;
  opterr = 0;
  optind = 0;
  //while (EOF != (ch = getopt(argc, argv, "csufpo:i:b:w:t:h"))) {
  while (EOF != (ch = getopt(argc, argv, "cuo:i:b:t:h"))) {
    switch (ch) {
    case 'c':
      compress = TRUE;
      strcpy(conf->infile, "test.txt");
      strcpy(conf->outfile, "out.cz");
      break;
#if 0
    case 'u':
      compress = FALSE;
      strcpy(conf->infile, "out.cz");
      strcpy(conf->outfile, "new.txt");
      break;
    case 'f': 
      conf->method = METHOD_FILE;
      break;
    case 'w':
      if (strcmp(optarg, "gzip") == 0)
        compress_way = COMPRESS_GZIP;
      else if (strcmp(optarg, "bzip2") == 0) 
        compress_way = COMPRESS_BZIP2;
      else compress_way = COMPRESS_NONE;
      break;
#endif
    case 'o':
      strcpy(conf->outfile, optarg);
      break;
    case 'i':
      strcpy(conf->infile, optarg);
      break;
    case 'h':
      usage(argv[0]);
      return -1;
#if 0
    case 'b':
      conf->b_size = atoi(optarg) * 1024;
      break;
    case 'p':
      conf->preloading = 1;
      break;
#endif
    case 't':
      conf->nthreads = atoi(optarg);
      break;
    case '?':
      fprintf(stdout, "Unknown option `-%c'.\n", optopt);
      usage(argv[0]);
      return -1;
    }
  }

  conf->compress_way = compress_way;
  if (compress) {
    Encode(conf);
  } else {
    printf("Only encode implemented\n");
    exit(1);
  }

#ifdef ENABLE_PARSEC_HOOKS
  __parsec_bench_end();
#endif

  tbb::tick_count t1 = tbb::tick_count::now();
  std::cout << "Input file: " << conf->infile << "\n" <<
      "Total Time: " << (t1-t0).seconds() << " sec\n\n";

  return 0;
}
