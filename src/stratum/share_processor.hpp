#ifndef SHARE_PROCESSOR_HPP
#define SHARE_PROCESSOR_HPP
#include <sw/redis++/redis++.h>

#include "share.hpp"
#include "share_result.hpp"
#include "stratum_client.hpp"

using namespace sw::redis;

class ShareProcessor{
    public:
     ShareProcessor(Redis *redisCli, std::string(*hash)(std::string));
     ShareResult ProcessShare(Share share);
     private:
     Redis* redis_cli;
     std::string(*hash)(std::string);
};

#endif