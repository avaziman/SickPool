#ifndef SHARE_HPP_
#define SHARE_HPP_
#include <string_view>
#include <vector>
struct Share{
    std::string_view worker;
    std::string_view jobId;
    std::string_view time;
    std::string_view nonce2;
    std::string_view solution;
};

enum class ShareCode
{
    UNKNOWN = 20,
    JOB_NOT_FOUND = 21,
    DUPLICATE_SHARE = 22,
    LOW_DIFFICULTY_SHARE = 23,
    UNAUTHORIZED_WORKER = 24,
    NOT_SUBSCRIBED = 25,

    // non-standard
    VALID_SHARE = 30,
    VALID_BLOCK = 31,
};

class ShareResult{
    public:
     ShareResult() : Code(ShareCode::UNKNOWN), HashBytes(32) {}
     ShareCode Code;
     const char* Message;
     double Diff;
     // uint256 takes vector as param
     std::vector<unsigned char> HashBytes;
};

struct BlockSubmission{
    std::vector<unsigned char> HashBytes;
    std::string worker;
};

#endif