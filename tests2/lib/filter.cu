#define _GNU_SOURCE

#include <stdexcept>
#include <getopt.h>
#include <cuda.h>
#include <utility>
#include <vector>
#include <string_view>
#include <iostream>
#include <functional>
#include <error.h>
#include <sstream>
#include <unistd.h>

static const char usage[] = "[OPTION]... [FILTER]... <command>...";
static const char help[] = R"EOF(Options:
Options:
  -v, --verbose           Print progress of the devices through the filters.
  -h, --help              Print this help message.

Filters:
  -c, --capability [QUAL]MAJOR[.MINOR]
                          Only allow CUDA devices with the given compute capability.
                          QUAL can be any of =, <, >, all of which are inclusive.
  -r, --random=[N]        Allow at most N CUDA devices chosen at random. Defaults to 1.
)EOF";

using devlist = std::vector<std::pair<int, cudaDeviceProp>>;

static devlist fetch_devices();
static void print_devlist(const devlist&);
static void filter_random(devlist&, const std::string&);
static void filter_capability(devlist&, const std::string&);

int main(int argc, char* const* argv) {
  if(getenv("CUDA_VISIBLE_DEVICES") != NULL) {
    std::cerr << "CUDA_VISIBLE_DEVICES is set, refusing to override\n";
    return 1;
  }

  devlist devs = fetch_devices();
  if(devs.empty()) {
    std::cerr << "No devices available!\n";
    return 77;  // SKIP
  }

  static const struct option opts[] = {
    {"help", no_argument, NULL, 'h'},
    {"capability", required_argument, NULL, 'c'},
    {"random", optional_argument, NULL, 'r'},
    {0, 0, 0, 0}
  };
  int opt;
  bool verbose = false;
  while((opt = getopt_long(argc, argv, "+hvc:r::", opts, nullptr)) != -1) {
    bool is_filter = false;
    switch(opt) {
    case 'h':
      std::cout << (argc > 0 ? argv[0] : "cuda-filter") << " " << usage << "\n\n" << help;
      return 0;
    case 'v':
      verbose = true;
      std::cerr << "Initial list of devices:\n";
      print_devlist(devs);
      break;
    case 'c':
      is_filter = true;
      if(verbose)
        std::cerr << "Applying --capability " << optarg << "\n";
      filter_capability(devs, optarg);
      break;
    case 'r':
      is_filter = true;
      if(verbose)
        std::cerr << "Applying --random " << (optarg ? optarg : "") << "\n";
      filter_random(devs, optarg ? optarg : "");
      break;
    default:
      std::cerr << "Usage: " << (argc > 0 ? argv[0] : "cuda-filter") << " " << usage << "\n";
      return 2;
    }

    if(is_filter) {
      if(verbose)
        print_devlist(devs);

      if(devs.empty()) {
        std::cerr << "No devices passed the filters!\n";
        return 77;  // SKIP
      }
    }
  }

  {
    std::ostringstream ss;
    bool first = true;
    for(const auto& dev: devs) {
      if(!first) ss << ",";
      first = false;
      ss << dev.first;
    }
    if(setenv("CUDA_VISIBLE_DEVICES", ss.str().c_str(), 1) != 0)
      error(1, errno, "Failed to setenv(CUDA_VISIBLE_DEVICES)");
  }

  std::vector<char*> new_argv;
  for(int i = optind; i < argc; i++) {
    new_argv.push_back(argv[i]);
  }
  if(new_argv.empty()) {
    std::cerr << "No command provided!\n";
    std::cerr << "Usage: " << (argc > 0 ? argv[0] : "cuda-filter") << " " << usage << "\n";
    return 2;
  }
  new_argv.push_back(NULL);

  execvp(new_argv[0], new_argv.data());
  error(127, errno, "Failed to exec");
  return 0;
}

devlist fetch_devices() {
  int nDevices = 0;
  cudaError_t err = cudaGetDeviceCount(&nDevices);
  if(err != cudaSuccess || nDevices == 0)
    return {};

  devlist result;
  result.reserve(nDevices);
  for(int i = 0; i < nDevices; i++) {
    cudaDeviceProp prop;
    err = cudaGetDeviceProperties(&prop, i);
    if(err != cudaSuccess) {
      std::cerr << "Error fetching properties for device " << i << "\n";
      continue;
    }

    result.push_back({i, prop});
  }
  return result;
}

void print_devlist(const devlist& devs) {
  for(const auto& dev: devs)
    std::cerr << "  - " << dev.second.name << "\n";
}

void filter_random(devlist& devs, const std::string& arg) {
  int n = 1;
  if(!arg.empty())
    n = std::stoi(arg, nullptr, 10);
  if(n < 1) {
    std::cerr << "Invalid argument to --random, must be >= 1!\n";
    std::exit(2);
  }

  devlist bucket = std::move(devs);
  devs.clear();
  for(int i = 0; !bucket.empty() && i < n; i++) {
    int idx = nearbyint(drand48() * (bucket.size() - 1));
    devs.emplace_back(bucket.at(idx));
  }
}

void filter_capability(devlist& devs, const std::string& fullarg) {
  std::string_view arg = fullarg;

  std::function<bool(int, int)> qual = std::equal_to<int>();
  switch(arg[0]) {
  case '=':
    qual = std::equal_to<int>();
    arg = arg.substr(1);
    break;
  case '<':
    qual = std::less_equal<int>();
    arg = arg.substr(1);
    break;
  case '>':
    qual = std::greater_equal<int>();
    arg = arg.substr(1);
    break;
  }

  std::size_t pos;
  int major = std::stoi(std::string(arg), &pos, 10);
  arg = arg.substr(pos);

  int minor = -1;
  if(!arg.empty()) {
    if(arg[0] != '.') {
      std::cerr << "Invalid argument to --capability: " << fullarg;
      std::exit(2);
    }
    arg = arg.substr(1);
    minor = std::stoi(std::string(arg), nullptr, 10);
  }

  devlist bucket = std::move(devs);
  devs.clear();
  for(auto& dev: bucket) {
    if(qual(dev.second.major, major)
       && (minor == -1 || dev.second.major != major || qual(dev.second.minor, minor))) {
      devs.emplace_back(std::move(dev));
    }
  }
}
