#include "Options.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

namespace dht {
Options::Options()
    : bind_ip(nullptr)
    , bootstrap() {
}

bool
parse(int argc, char **argv, Options &result) noexcept {
  int verbose_flag;
  static const option long_options[] = //
      {
          // long name|has args|where to store|what value it will get
          {"verbose", no_argument, &verbose_flag, 1},
          {"brief", no_argument, &verbose_flag, 0},
          /* These options don’t set a flag.
             We distinguish them by their indices. */
          // {"add", no_argument, nullptr, 'a'},
          {"bind", required_argument, nullptr, 'b'},
          {"bootstrap", required_argument, nullptr, 'o'},
          // {"delete", required_argument, nullptr, 'd'},
          // {"create", required_argument, nullptr, 'c'},
          {"fifo", required_argument, nullptr, 'f'},
          //  The last element of the array has to be filled with zeros
          {nullptr, 0, nullptr, 0} //
      };

  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "abc:d:f:", long_options, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
    case 0:
      /* If this option set a flag, do nothing else now. */
      if (long_options[option_index].flag != 0) {
        break;
      }

      printf("option %s", long_options[option_index].name);
      if (optarg) {
        printf(" with arg %s", optarg);
      }
      printf("\n");
      break;
    case 'a':
      puts("option -a\n");
      break;

    case 'b':
      puts("option -b\n");
      break;

    case 'c':
      printf("option -c with value `%s'\n", optarg);
      break;

    case 'd':
      printf("option -d with value `%s'\n", optarg);
      break;

    case 'f':
      printf("option -f with value `%s'\n", optarg);
      break;

    case '?':
      /* getopt_long already printed an error message. */
      break;

    default:
      printf("default:\n");
      return false;
    }
  } // while

  return true;
}

} // namespace dht
