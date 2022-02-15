#ifndef REDIS_MANAGER_HPP
#define REDIS_MANAGER_HPP
#include <sw/redis++/redis++.h>
#include <iostream>
#include <chrono>

using namespace sw::redis;

class RedisManager{
    public:
     RedisManager(std::string symbol, std::string host) : symbol(symbol){
         redis_client = new Redis("tcp://" + host);
     }

     void InsertPendingBlock(std::string blockHash){
         redis_client->rpush("blocks", blockHash);
     }

     uint32_t GetJobId()
     {
         auto res = redis_client->get(symbol + ":job_id");
         if(!res.has_value())
         {
             redis_client->set(symbol + ":job_id", "0");
             std::cout << "no job_id found in db, set to 0." << std::endl;
             return 0;
         }
         return (uint32_t)std::stoul(res.value());
     }

     void SetJobId(uint32_t jobId){
         redis_client->set(symbol + ":job_id", std::to_string(jobId));
     }

     void AddShare(std::string worker, double diff){
         auto now = std::chrono::system_clock::now();
         int64_t unixNow = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        
         std::string shareData = worker + ":" + std::to_string(diff);
         redis_client->zadd(symbol + ":shares", shareData, unixNow);
     }

     private:
      Redis* redis_client;
      std::string symbol;
};

#endif