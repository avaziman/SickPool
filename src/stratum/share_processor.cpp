#include "share_processor.hpp"

ShareProcessor::ShareProcessor(Redis* redis, std::string(*hash)(std::string)) : redis_cli(redis), hash(hash){}

ShareResult ShareProcessor::ProcessShare(Share share){
    
}