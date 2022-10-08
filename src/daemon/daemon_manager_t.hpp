#ifndef DAEMON_MANAGER_T_HPP_
#define DAEMON_MANAGER_T_HPP_

#include "static_config.hpp"

#if SICK_COIN == SIN
#include "daemon_manager_sin.hpp"
using daemon_manager_t = DaemonManagerSin;
#elif SICK_COIN == ZANO
#include "daemon_manager_zano.hpp"
using daemon_manager_t = DaemonManagerZano;
#endif

#endif