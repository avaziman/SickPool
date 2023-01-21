#ifndef DIFFICULTY_MANAGER_HPP_
#define DIFFICULTY_MANAGER_HPP_

#include <map>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "coin_config.hpp"
#include "connection.hpp"
#include "functional"
#include "logger.hpp"

// allows having different share rate targets for different miners;
class VarDiff
{
   private:
    // default initialize, spent 2 hours on this...
    std::array<uint64_t, 10> share_times{};
    uint64_t retarget_count = 0;
    double retarget_sum = 0.0;
    uint64_t sum = 0;
    int pos = 0;
    const double target_share_rate;
    const uint32_t retarget_interval;

   public:
    explicit VarDiff(double rate, uint32_t retarget_interval)
        : target_share_rate(rate), retarget_interval(retarget_interval)
    {
    }

    void Add(uint64_t t)
    {
        if (pos >= share_times.size()) pos = 0;

        sum += t;
        sum -= share_times[pos];  // oldest value
        share_times[pos] = t;

        pos++;
    }

    double Adjust(double current_diff, uint64_t curtime, uint64_t last_adjusted)
    {
        // first retarget of first 5 shares
        if ((curtime - last_adjusted) / 1000 < retarget_interval)
        {
            if (retarget_count == 0)
            {
                if (pos < 5)
                {
                    return 0.0;
                }
            }
            else
            {
                if (pos < share_times.size())
                {
                    return 0.0;
                }
            }
        }

        const double current_share_time =
            static_cast<double>(sum) / static_cast<double>(pos) / 1000.0;

        retarget_sum += current_share_time;
        retarget_count++;

        // sum can't be zero because its called on share

        // average minute rate of averages
        const double minute_rate =
            60.0 / (retarget_sum / static_cast<double>(retarget_count));

        const double diff_multiplier = minute_rate / target_share_rate;
        double new_diff = current_diff * diff_multiplier;

        const double variance = std::abs(new_diff - current_diff);

        if (const double variance_ratio = variance / current_diff;
            variance_ratio > 0.1)
        {
            logger.Log<LogType::Debug>(
                "Adjusted difficulty from {} to {}, average share rate: {}, "
                "current share rate: {}",
                current_diff, new_diff, minute_rate, 60.0 / current_share_time);

            return new_diff;
        }
        return 0.0;
    }
    static constexpr std::string_view field_str_vardiff = "VarDiff";

    const static Logger<field_str_vardiff> logger;

    // void Adjust(const int passed_seconds, const int64_t curtime_ms) const
    // {
    //     std::shared_lock read_lock(*clients_mutex);
    //     for (auto& [conn, _] : *clients)
    //     {
    //         StratumClient* client = conn->ptr.get();
    //         Adjust(client, passed_seconds, curtime_ms);
    //     }
    // }

    // void Adjust(StratumClient* client, int passed_seconds,
    //             int64_t curtime_ms) const
    // {
    //     // client hasn't been connected for long enough
    //     if (curtime_ms - client->GetLastAdjusted() < passed_seconds) return;

    //     const double current_diff = client->GetDifficulty();
    //     const double minute_rate =
    //         static_cast<double>(client->GetShareCount()) /
    //         (passed_seconds / 60.0);

    //     const double diff_multiplier = minute_rate / target_share_rate;

    //     double new_diff = current_diff * diff_multiplier;
    //     const double variance = std::abs(new_diff - current_diff);

    //     const double variance_ratio = variance / current_diff;

    //     if (minute_rate == 0)
    //     {
    //         new_diff = current_diff / 10;
    //     }

    //     if (variance_ratio > 0.1)
    //     {
    //         client->SetPendingDifficulty(new_diff);
    //         logger.Log<LogType::Debug>(
    //             "Adjusted difficulty for {} from {} to {}, share rate: {}",
    //             client->GetFullWorkerName(), current_diff, new_diff,
    //             minute_rate);
    //     }
    // }
};

#endif