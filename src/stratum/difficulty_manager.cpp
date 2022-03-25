// #include "difficul   y_manager.hpp"

// // std::mutex DifficultyManager::clients_mutex;

// void DifficultyManager::Start(std::mutex& mutex)
// {
//     while (true)
//     {
//         std::this_thread::sleep_for(std::chrono::seconds(UPDATE_INTERVAL));
//         std::cerr << "len: " << clients.size() << std::endl;

//         mutex.lock();

//         for (StratumClient* cli : clients)
//         {
//         }
//         mutex.unlock();

//         std::this_thread::sleep_for(std::chrono::seconds(UPDATE_INTERVAL));
//     }
// }