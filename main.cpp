#define _CRT_SECURE_NO_WARNINGS  // avoid some unsafe functions from getopt.h
                                 // error

#define WIN32_LEAN_AND_MEAN  // boost on windows

#include <stdio.h>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "ChatServer.h"
#include "ChatServerConfig.h"
#include "getopt.h" /* getopt_long */

int main(int argc, char **argv) {
  ChatServerConfig config;
  config.ip = "127.0.0.1";
  config.port = 1234;
  config.workers = 1;
  config.userlimit = 5;
  config.verbose = true;
  parse_console_parameters(argc, argv, config);

  if (!help_opt) {
    try {
      boost::thread_group threads_;  // thread_pool
      boost::asio::io_context io_context;
      boost::asio::io_context::work work_(io_context);
      for (std::size_t i = 0; i < config.workers; ++i) {
          threads_.create_thread(
              boost::bind(&boost::asio::io_context::run, &io_context));
      }
      tcp::endpoint endpoint(tcp::v4(), config.port);
      chat_server server(io_context, endpoint, config);

      io_context.run();
    } catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
    }
  }
  return 0;
}

void parse_console_parameters(int argc, char **argv, ChatServerConfig &config) {
  static struct option long_options[] = {
      {"ip", required_argument, 0, 'i'},
      {"port", required_argument, 0, 'p'},
      {"userlimit", required_argument, 0, 'u'},
      {"workers", required_argument, 0, 'w'},
      {"verbose", no_argument, 0, 'v'},  // 0
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int c, option_index = 0;
  while (-1 != (c = getopt_long(argc, argv, "i:p:u:w:v:h", long_options,
                                &option_index))) {
    switch (c) {
      case 0:
        printf("option %s", long_options[option_index].name);
        if (optarg) printf(" with arg %s", optarg);
        printf("\n");
        switch (option_index) {
          case 0:
            config.ip = optarg;
            break;
          case 1:
            config.port = std::stoi(optarg);
            break;
          case 2:
            config.userlimit = std::stoi(optarg);
            break;
          case 3:
            config.workers = std::stoi(optarg);
            break;
          case 4:
            config.verbose = true;  // optarg;
            break;
          case 5:
            help_opt = true;
            printf(
                "using:\n\t./exe -i|--ip <ip> -p|--port <port> "
                "-u|--userlimit <uint> -w|--workers <num> "
                "[-v|--verbose <uint>] [-h|--help <uint>]\n\n");
            break;
          case 6:
            printf(
                "using:\n\t./exe -i|--ip <ip> -p|--port <port> "
                "-u|--userlimit <uint> -w|--workers <num> "
                "[-v|--verbose <uint>] [-h|--help <uint>]\n\n");
            break;
        }
        break;

      case 'i':
        config.ip = optarg;
        break;
      case 'p':
        config.port = std::stoi(optarg);
        break;
      case 'u':
        config.userlimit = std::stoi(optarg);
        break;
      case 'w':
        config.workers = std::stoi(optarg);
        break;
      case 'v':
        config.verbose = std::stoi(optarg);
        break;
      case 'h':
        help_opt = true;
        printf(
            "using:\n\t./exe -i|--ip <ip> -p|--port <port> "
            "-u|--userlimit <uint> -w|--workers <num> "
            "[-v|--verbose <uint>] [-h|--help <uint>]\n\n");
        break;

      case '?': /* getopt_long already printed an error message. */
        printf(
            "using:\n\t./exe -i|--ip <ip> -p|--port <port> "
            "-u|--userlimit <uint> -w|--workers <num> "
            "[-v|--verbose <uint>] [-h|--help <uint>]\n\n");
        break;

      default:
        printf("?? getopt returned character code 0%o ??\n", c);
        break;
    }
  }
  if (optind < argc) {
    printf("non-option argv elements: ");
    while (optind < argc) printf("%s ", argv[optind++]);
    printf("\n");
  }
}