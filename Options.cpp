#include "Options.h"
#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <io/file.h>

static const char *def_dump_path = "/tmp/dht_db.dump2";

namespace dht {
Options::Options()
    : port(42605)
    , bootstrap()
    , dump_file{0} {
  std::memcpy(dump_file, def_dump_path, strlen(def_dump_path));
}

bool
parse(Options &self, int argc, char **argv) noexcept {
  int verbose_flag;
  static const option long_options[] = //
      {
          // long name|has args|where to store|what key it will get?
          {"verbose", no_argument, &verbose_flag, 1},
          {"brief", no_argument, &verbose_flag, 0},
          /* These options don’t set a flag.
             We distinguish them by their indices. */
          // {"add", no_argument, nullptr, 'a'},
          {"bind", required_argument, nullptr, 'b'},
          {"bootstrap", required_argument, nullptr, 'o'},
          {"db", required_argument, nullptr, 'd'},
          // {"delete", required_argument, nullptr, 'd'},
          // {"create", required_argument, nullptr, 'c'},
          {"fifo", required_argument, nullptr, 'f'},
          {"help", no_argument, nullptr, 'h'},
          //  The last element of the array has to be filled with zeros
          {nullptr, 0, nullptr, 0} //
      };

  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "b:o:f:h:d", long_options, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
    case 0:
    case 1:
      /* If this option set a flag, do nothing else now. */
      if (long_options[option_index].flag != 0) {
        // break;
      }

      printf("option %s", long_options[option_index].name);
      if (optarg) {
        printf(" with arg %s", optarg);
      }
      printf("\n");
      break;

    case 'b':
      if (!convert(optarg, self.port)) {
        printf("option -b with value `%s'\n", optarg);
        return false;
      }
      break;

    case 'o':
      printf("option -o with value `%s'\n", optarg);
      break;

    case 'd': {
      std::size_t len = std::strlen(optarg);
      std::size_t max = sizeof(self.dump_file);

      if (len >= max) {
        fprintf(stderr, "To long '%s':%zu max: %zu\n", optarg, len, max);
        return 1;
      }
      std::memcpy(self.dump_file, optarg, len + 1);
    } break;

    case 'f':
      printf("option -f with value `%s'\n", optarg);
      break;

    case 'h':
      printf("option -h\n");
      return false;
      break;

    case '?':
      /* getopt_long already printed an error message. */
      break;

    default:
      printf("default:\n");
      return false;
    }
  } // while

  if (!fs::is_file(self.dump_file)) {
    fprintf(stderr, "Unexisting '%s'", self.dump_file);
  }

  return true;
}

} // namespace dht
