#ifndef DIFFICULTY_MANAGER_HPP_
#define DIFFICULTY_MANAGER_HPP_

#include <mutex>
#include <thread>
#include <vector>

#include "stratum_client.hpp"

class DifficultyManager
{
   public:
    DifficultyManager(double targetSharesRate, )
        : target_shares_rate(targetSharesRate), clients(clients)
    {
    }

    void Adjust(double target_share_rate,
                std::vector<std::unique_ptr<StratumClient>>& clients)
    {
        for (auto& client : clients)
        {
            const double current_diff = client->GetDifficulty();
            const double minute_rate =
                client->GetShareCount() / (passed_seconds / 60);

            const double diff_multiplier = target_share_rate / minute_rate;

            const double new_diff = current_diff * diff_multiplier;
            const double variance = std::abs(new_diff - current_diff);

            const double variance_ratio = variance / current_diff;

            if (variance_ratio > 0.1)
            {
                client->SetPendingDifficulty(new_diff);
            }
        }
    }

    // static std::mutex clients_mutex;

    void Start(std::mutex& mutex);

   private:
    double target_shares_rate;
    std::vector<StratumClient*>& clients;
};

#endif