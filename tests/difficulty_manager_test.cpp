// #include "difficulty_manager.hpp"

// #include <gtest/gtest.h>

// #include "connection.hpp"
// #include "stratum_client.hpp"

// double GetNextDiff(const int time_period, const double target_shares,
//                    const int shares_done, const double start_diff)
// {
//     in_addr garbage_addr;
//     std::shared_mutex useless_mutex;

//     std::map<std::shared_ptr<Connection<StratumClient>>, double> clients = {
//         {std::make_shared<Connection<StratumClient>>(1, garbage_addr), 0.0}};

//     auto conn = (*clients.begin()).first;
//     conn->ptr = std::make_shared<StratumClient>(0, start_diff);

//     for (int i = 0; i < shares_done; i++) conn->ptr->SetLastShare(0, 0);

//     DifficultyManager diff_manager(&clients, &useless_mutex, target_shares);
//     diff_manager.Adjust(time_period, 1 + time_period);
//     return conn->ptr->GetPendingDifficulty();
// }

// // target: 100 shares a minute
// // starting diff: 50
// // client has done 200 shares in 1 minute
// // expect difficulty to be doubled
// TEST(DifficultyManagerTest, DoubleDiff)
// {
//     const double start_diff = 50;
//     const double target_shares = 100;
//     const int shares_done = 200;
//     const int time_period = 60;

//     double next_diff =
//         GetNextDiff(time_period, target_shares, shares_done, start_diff);
//     ASSERT_EQ(next_diff, start_diff * 2);
// }

// // target: 100 shares a minute
// // starting diff: 50
// // client has done 50 shares in 1 minute
// // expect difficulty to be halved
// TEST(DifficultyManagerTest, HalfDiff)
// {
//     const double start_diff = 50;
//     const double target_shares = 100;
//     const int shares_done = 50;
//     const int time_period = 60;

//     double next_diff =
//         GetNextDiff(time_period, target_shares, shares_done, start_diff);

//     ASSERT_EQ(next_diff, start_diff / 2);
// }

// // target: 100 shares a minute
// // starting diff: 50
// // client has done 95 shares in 1 minute
// // expect difficulty not to change
// TEST(DifficultyManagerTest, NoChange)
// {
//     const double start_diff = 50;
//     const double target_shares = 100;
//     const int shares_done = 95;
//     const int time_period = 60;

//     double next_diff =
//         GetNextDiff(time_period, target_shares, shares_done, start_diff);

//     ASSERT_EQ(next_diff, start_diff);
// }

// // target: 100 shares a minute
// // starting diff: 50
// // client has done 0 shares in 1 minute
// // expect difficulty not to change
// TEST(DifficultyManagerTest, DivBy10)
// {
//     const double start_diff = 50;
//     const double target_shares = 100;
//     const int shares_done = 0;
//     const int time_period = 60;

//     double next_diff =
//         GetNextDiff(time_period, target_shares, shares_done, start_diff);

//     ASSERT_EQ(next_diff, start_diff / 10);
// }