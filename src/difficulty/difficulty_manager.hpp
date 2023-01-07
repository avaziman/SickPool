#ifndef DIFFICULTY_MANAGER_HPP_
#define DIFFICULTY_MANAGER_HPP_

#include <map>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "connection.hpp"
#include "functional"
#include "logger.hpp"
#include "stratum_client.hpp"
#include "coin_config.hpp"

class DifficultyManager
{
   public:
    DifficultyManager(
        std::map<std::shared_ptr<Connection<StratumClient>>, double>* clients,
        std::shared_mutex* clients_mutex, const DifficultyConfig& diff_conf)
        : clients(clients),
          clients_mutex(clients_mutex),
          target_share_rate(diff_conf.target_shares_rate)
    {
    }

    void Adjust(const int passed_seconds, const int64_t curtime_ms) const
    {
        std::shared_lock read_lock(*clients_mutex);
        for (auto& [conn, _] : *clients)
        {
            StratumClient* client = conn->ptr.get();
            Adjust(client, passed_seconds, curtime_ms);
        }
    }

    void Adjust(StratumClient* client, int passed_seconds, int64_t curtime_ms) const {
        // client hasn't been connected for long enough
        if (curtime_ms - client->GetLastAdjusted() < passed_seconds) return;

        const double current_diff = client->GetDifficulty();
        const double minute_rate =
            static_cast<double>(client->GetShareCount()) /
            (passed_seconds / 60.0);

        const double diff_multiplier = minute_rate / target_share_rate;

        double new_diff = current_diff * diff_multiplier;
        const double variance = std::abs(new_diff - current_diff);

        const double variance_ratio = variance / current_diff;

        if (minute_rate == 0)
        {
            new_diff = current_diff / 10;
        }

        if (variance_ratio > 0.1)
        {
            client->SetPendingDifficulty(new_diff);
            logger.Log<LogType::Debug>(
                "Adjusted difficulty for {} from {} to {}, share rate: {}",
                client->GetFullWorkerName(), current_diff, new_diff,
                minute_rate);
        }
    }

   private:
    static constexpr std::string_view field_str = "DiffManager";
    Logger<field_str> logger;
    const double target_share_rate;
    std::map<std::shared_ptr<Connection<StratumClient>>, double>* clients;
    std::shared_mutex* clients_mutex;
};

#endif