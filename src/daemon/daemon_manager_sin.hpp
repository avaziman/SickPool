#ifndef DAEMON_MANAGER_SIN_HPP
#define DAEMON_MANAGER_SIN_HPP

#include "daemon_manager.hpp"
#include "daemon_responses_sin.hpp"

class DaemonManagerSin : public DaemonManager
{
   public:
    bool GetBlockTemplate(BlockTemplateRes& templateRes,
                          simdjson::ondemand::parser& parser);
};
#endif