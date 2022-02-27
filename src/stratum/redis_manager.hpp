#ifndef REDIS_MANAGER_HPP_
#define REDIS_MANAGER_HPP_
#include <sw/redis++/redis++.h>

#include <ctime>
#include <vector>
#include <unordered_map>
#include <iterator>

using namespace sw::redis;

class RedisManager
{
   public:
    RedisManager(std::string coinSymbol, std::string host)
        : redis("tcp://" + host), coin_symbol(coinSymbol)
    {
        
    }
    void AddShare(std::string worker, double diff, std::time_t time)
    {
        auto res = redis.zadd(coin_symbol + ":shares",
                              worker + ":" + std::to_string(diff), time);

        if (res < 1)
        {
            std::cerr << "Redis: Failed to submit share! (1)" << std::endl;
        }

        // only include valid shares
        if (diff > 0)
        {
            std::string address = worker.substr(0, worker.find('.'));
            res = redis.hincrbyfloat(coin_symbol + ":current_round", address,
                                     diff);

            if (res < 1)
            {
                std::cerr << "Redis: Failed to submit share! (2)" << std::endl;
            }
        }
    }

    void EndRound(uint32_t height)
    {
        std::string finishedRound =
            coin_symbol + ":round:" + std::to_string(height);
        redis.rename(coin_symbol + ":current_round", finishedRound);

        // append the balances
        std::vector<std::pair<std::string, double>> result;

        redis.hgetall("VRSC:current_round",
                      std::inserter(result, result.end()));

        double totalWork = 0;
        for (int i = 0; i < result.size(); i++)
            totalWork += result[i].second;

        for (int i = 0; i < result.size(); i++)
        {
            // PROP
            std::string miner = result[i].first;
            double rewardShare = totalWork / result[i].second;
            redis.zadd(coin_symbol + ":payments",
                       miner + ":" + std::to_string(rewardShare), height);

            std::cout << miner << " did " << rewardShare << "% of the work"
                      << std::endl;
        }
    }

    uint32_t GetJobCount()
    {
        Optional<std::string> val = redis.get(coin_symbol + ":job_count");
        if (val)
        {
            return static_cast<uint32_t>(std::stoul(*val));
        }
        return 0;
    }

    void SetJobCount(uint32_t jobCount)
    {
        bool res =
            redis.set(coin_symbol + ":job_count", std::to_string(jobCount));
        if (!res) std::cerr << "Redis: Failed to update job count" << std::endl;
    }

   private:
    Redis redis;
    std::string coin_symbol;
};

#endif