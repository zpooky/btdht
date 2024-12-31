#include "Options.h"
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <getopt.h>
#include <io/file.h>
#include <unistd.h>

static const char *default_dump_path = "./dht_db.dump";

namespace dht {
Options::Options()
    : port(34329)
    , bootstrap()
    , dump_file{0}
    , local_socket{0}
    , publish_socket{0}
    , db_path{0}
    , systemd{false} {
  memcpy(dump_file, default_dump_path, strlen(default_dump_path));
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
          {"local", required_argument, nullptr, 'l'},
          {"help", no_argument, nullptr, 'h'},
          {"systemd", no_argument, nullptr, 's'},
          //  The last element of the array has to be filled with zeros
          {nullptr, 0, nullptr, 0} //
      };

  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "b:o:l:hd", long_options, &option_index);
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
      if (!to_port(optarg, self.port)) {
        printf("option -b with value `%s'\n", optarg);
        return false;
      }
      break;

    case 'o':
      // printf("option -o with value `%s'\n", optarg);
      {
        Contact con;
        // XXX support hostname
        if (!to_contact(optarg, con)) {
          fprintf(stderr, "invalid bootstrap option '%s'", optarg);
          return false;
        }
        insert(self.bootstrap, con);
      }
      break;

    case 'd': {
      std::size_t len = std::strlen(optarg);
      std::size_t max = sizeof(self.dump_file);

      if (len >= max) {
        fprintf(stderr, "To long '%s':%zu max: %zu\n", optarg, len, max);
        return 1;
      }
      memcpy(self.dump_file, optarg, len + 1);
    } break;

    case 'l':
      printf("option -l with value `%s'\n", optarg);
      break;

    case 's':
      printf("option -s\n");
      self.systemd = true;
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

  if (strlen(self.local_socket) == 0) {
    if (!xdg_runtime_dir(self.local_socket)) {
      return false;
    }
    mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR;
    if (!fs::mkdirs(self.local_socket, mode)) {
      return false;
    }
    ::strcat(self.local_socket, "/spdht.socket");
  }
  unlink(self.local_socket);

  if (strlen(self.publish_socket) == 0) {
    if (!xdg_runtime_dir(self.publish_socket)) {
      return false;
    }
    mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR;
    if (!fs::mkdirs(self.publish_socket, mode)) {
      return false;
    }
    ::strcat(self.publish_socket, "/spdht_publish.socket");
  }
  unlink(self.publish_socket);

  if (strlen(self.db_path) == 0) {
    if (!xdg_share_dir(self.db_path)) {
      return false;
    }
    ::strcat(self.db_path, "/spbt/torrent.db");
  }

  if (strlen(self.scrape_socket_path) == 0) {
    if (!xdg_runtime_dir(self.scrape_socket_path)) {
      return false;
    }
    mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR;
    if (!fs::mkdirs(self.scrape_socket_path, mode)) {
      return false;
    }
  }

  return true;
}

} // namespace dht
