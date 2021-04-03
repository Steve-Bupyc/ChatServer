#pragma once
#include <string>

/*
    ip          - IP address of server listener
    port        - Port of server listener
    userlimit   - Max number of users allowed
    workers     - Number of threads
    verbose     - Flag that indicates that debug messages is printed to stdout
                  (stderr), if not set server prints only errors help Print help string
*/
struct ChatServerConfig {
  std::string ip;
  int port;
  int userlimit;
  int workers;
  bool verbose;
};