// #ifndef DIFFICULTY_MANAGER_HPP_
// #define DIFFICULTY_MANAGER_HPP_
// // #define UPDATE_INTERVAL 15

// #include <mutex>
// #include <thread>
// #include <vector>

// #include "stratum_client.hpp"

// class DifficultyManager
// {
//    public:
//     DifficultyManager(double targetSharesRate,
//                       std::vector<StratumClient*>& clients)
//         : target_shares_rate(targetSharesRate), clients(clients)
//     {
        
//     }

//     // static std::mutex clients_mutex;

//     void Start(std::mutex& mutex);

//    private:
//     double target_shares_rate;
//     std::vector<StratumClient*>& clients;
// };

// #endif