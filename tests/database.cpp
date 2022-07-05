#include <gtest/gtest.h>

#include "stratum/job_manager.hpp"
#include "stratum/redis_manager.hpp"

TEST(DATABASE, AddBlockSubmission)
{
    using namespace std::string_view_literals;

    RedisManager redis_manager("127.0.0.1", 6397);
    DaemonManager daemon_manager(
        {RpcConfig{.host = "127.0.0.1:6004", .auth = "YWRtaW4xOnBhc3MxMjM="}});
    JobManager job_manager(&daemon_manager,
                           "RSicKPooLFbBeWZEgVrAkCxfAkPRQYwSnC");

    ShareResult share_res;
    share_res.Diff = 1;
    share_res.HashBytes = std::vector<unsigned char>(0, 32);

    Round round{.pow = 10, .round_start_ms = 0};

    const job_t* job = job_manager.GetNewJob();
    BlockSubmission submission("GTEST"sv, "GTEST_WORKER"sv, job, share_res,
                               round, 1, 1);

    redis_manager.AddBlockSubmission(&submission);
}