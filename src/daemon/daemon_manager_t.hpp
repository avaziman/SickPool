#ifndef DAEMON_MANAGER_T_HPP_
#define DAEMON_MANAGER_T_HPP_

#if COIN == SIN
#include "daemon_manager_sin.hpp"
using daemon_manager_t = DaemonManagerSin;
#endif

#endif