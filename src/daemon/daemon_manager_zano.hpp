#ifndef DAEMON_MANAGER_ZANO_HPP
#define DAEMON_MANAGER_ZANO_HPP

#include "daemon_manager.hpp"
#include "daemon_responses_sin.hpp"

class DaemonManagerZano : public DaemonManager
{
   public:
    bool GetBlockTemplate(BlockTemplateRes& templateRes,
                          simdjson::ondemand::parser& parser);
};
#endif