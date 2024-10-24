/*
 * Copyright ©2023 Chris Thachuk.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Fall Quarter 2023 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdio.h>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <string.h>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"

using std::string;

extern "C" {
  #include "libhw1/CSE333.h"
}

namespace hw4 {

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int* const listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // STEP 1:
  // Populate the  "hints" addrinfo structure for getaddrinfo()
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = ai_family;
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "in6addr_any" address
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  struct addrinfo* result;
  char portnum[80];
  int str_len = sprintf(portnum, "%d", port_);
  int res = getaddrinfo(nullptr, portnum, &hints, &result);

  // Check if getaddrinfo() failed.
  if (res != 0) {
    std::cerr << "getaddrinfo() failed: " << gai_strerror(res) << std::endl;
    return -1;
  }

  // Loop through the returned address structures until we are able
  // to create a socket and bind to one.
  for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    *listen_fd = socket(rp->ai_family,
                       rp->ai_socktype,
                       rp->ai_protocol);
    if (*listen_fd == -1) {
      // Creating this socket failed.  So, loop to the next returned
      // result and try again.
      std::cerr << "socket() failed: " << strerror(errno) << std::endl;
      *listen_fd = -1;
      continue;
    }

    // Configure the socket
    int optval = 1;
    setsockopt(*listen_fd, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof(optval));

    // Try binding the socket to the address and port number returned
    // by getaddrinfo().
    if (bind(*listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      // Bind worked; return the address family.
      sock_family_ = rp->ai_family;
      break;
    }

    // The bind failed.
    close(*listen_fd);
    *listen_fd = -1;
  }

  // Free the structure returned by getaddrinfo().
  freeaddrinfo(result);

  // If we failed to bind, return failure.
  if (*listen_fd <= 0)
    return false;

  // Success. Tell the OS that we want this to be a listening socket.
  if (listen(*listen_fd, SOMAXCONN) != 0) {
    std::cerr << "Failed to mark socket as listening: " << strerror(errno)
              << std::endl;
    close(*listen_fd);
    return false;
  }

  listen_sock_fd_ = *listen_fd;

  return true;
}

bool ServerSocket::Accept(int* const accepted_fd,
                          std::string* const client_addr,
                          uint16_t* const client_port,
                          std::string* const client_dns_name,
                          std::string* const server_addr,
                          std::string* const server_dns_name) const {
  // Accept a new connection on the listening socket listen_sock_fd_.
  // (Block until a new connection arrives.)  Return the newly accepted
  // socket, as well as information about both ends of the new connection,
  // through the various output parameters.

  // STEP 2:
  int client_fd;
  struct sockaddr_storage caddr;
  socklen_t caddr_len = sizeof(caddr);
  client_fd = accept(listen_sock_fd_,
                         reinterpret_cast<struct sockaddr*>(&caddr),
                         &caddr_len);
  if (client_fd < 0) {
    if ((errno == EAGAIN) || (errno == EINTR)) {
      std::cerr << "Failure on accept: " << strerror(errno) << std::endl;
      close(listen_sock_fd_);
      return false;
    }
    return false;
  }

  *accepted_fd = client_fd;

  if (caddr.ss_family == AF_INET) {
    char astring[INET_ADDRSTRLEN];
    struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(&caddr);
    inet_ntop(AF_INET, &(sa->sin_addr), astring, INET_ADDRSTRLEN);
    *client_addr = astring;

    *client_port = sa->sin_port;
  } else {  // if (caddr.ss_family == AF_INET6) {
    char astring[INET6_ADDRSTRLEN];
    struct sockaddr_in6* sa6 = reinterpret_cast<struct sockaddr_in6*>(&caddr);
    inet_ntop(AF_INET6, &(sa6->sin6_addr), astring, INET6_ADDRSTRLEN);
    *client_addr = astring;

    *client_port = sa6->sin6_port;
  }

  char hostname[NI_MAXHOST];

  getnameinfo(reinterpret_cast<struct sockaddr*>(&caddr),
                      sizeof (caddr),
                      hostname,
                      NI_MAXHOST,
                      nullptr,
                      0,
                      0);

  *client_dns_name = hostname;

  if (sock_family_ == AF_INET) {
    struct sockaddr_in sa;
    socklen_t len = sizeof(struct sockaddr_in);
    getsockname(client_fd, reinterpret_cast<struct sockaddr*>(&sa), &len);
    char astring[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sa.sin_addr), astring, INET_ADDRSTRLEN);

    *server_addr = astring;

    getnameinfo(reinterpret_cast<struct sockaddr*>(&sa),
                        sizeof (struct sockaddr),
                        hostname,
                        NI_MAXHOST,
                        nullptr,
                        0,
                        0);
  } else {  // if (sock_family_ == AF_INET6) {
    struct sockaddr_in6 sa6;
    socklen_t len = sizeof(struct sockaddr_in6);
    getsockname(client_fd, reinterpret_cast<struct sockaddr*>(&sa6), &len);
    char astring[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(sa6.sin6_addr), astring, INET6_ADDRSTRLEN);

    *server_addr = astring;

    getnameinfo(reinterpret_cast<struct sockaddr*>(&sa6),
                        sizeof (struct sockaddr),
                        hostname,
                        NI_MAXHOST,
                        nullptr,
                        0,
                        0);
  }

  *server_dns_name = hostname;

  return true;
}

}  // namespace hw4
