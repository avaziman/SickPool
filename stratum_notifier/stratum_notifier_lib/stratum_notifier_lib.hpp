#ifndef STRATUM_NOTIFIER_LIB_HPP
#define STRATUM_NOTIFIER_LIB_HPP

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  //close

#include <cstdio>

int stratum_notify(const char* method, const char* param);

#endif