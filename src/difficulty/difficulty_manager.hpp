#ifndef DIFFICULTY_MANAGER_HPP_
#define DIFFICULTY_MANAGER_HPP_

#include <mutex>
#include <thread>
#include <map>
#include <vector>

#include "logger.hpp"
#include "stratum_client.hpp"
#include "connection.hpp"

class DifficultyManager
{
   public:
    DifficultyManager(
        std::map<Connection<StratumClient>*, double>* clients, std::mutex* clients_mutex,
        double targetSharesRate)
        : clients(clients), clients_mutex(clients_mutex), target_share_rate(targetSharesRate)
    {
    }

    void Adjust(const int passed_seconds)
    {
        std::scoped_lock lock(*clients_mutex);
        for (auto& [conn, _] : *clients)
        {
            StratumClient* client = conn->ptr.get();
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
                Logger::Log(
                    LogType::Debug, LogField::DiffManager,
                    "Adjusted difficulty for {} from {} to {}, share rate: {}",
                    client->GetFullWorkerName(), current_diff, new_diff,
                    target_share_rate);
            }
        }
    }

    // static std::mutex clients_mutex;

   private:
    double target_share_rate;
    std::map<Connection<StratumClient>*, double>* clients;
    std::mutex* clients_mutex;
};

#endif