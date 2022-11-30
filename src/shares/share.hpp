#ifndef SHARE_HPP_
#define SHARE_HPP_
#include <string_view>
#include <memory>
#include <vector>
#include <simdjson.h>

#include "static_config.hpp"

template <StratumProtocol sp>
struct StratumShare{};

template <> 
struct StratumShare<StratumProtocol::ZEC>
{
    // as in order of appearance
    std::string_view worker;
    std::string_view job_id;
    std::string_view time_sv;
    std::string_view nonce2;
    std::string_view solution;

    uint32_t time;
};
using ShareZec = StratumShare<StratumProtocol::ZEC>;

template <>
struct StratumShare<StratumProtocol::BTC>
{
    // as in order of appearance
    std::string_view worker;
    std::string_view job_id;
    std::string_view extranonce2;
    std::string_view time_sv;
    std::string_view nonce_sv;

    uint32_t time;
    uint32_t nonce;
};
using ShareBtc = StratumShare<StratumProtocol::BTC>;

template <>
struct StratumShare<StratumProtocol::CN>
{
    std::string_view worker;
    
    std::string_view nonce_sv;
    // used to identify job instead of JobId.
    std::string_view job_id; // header_pow
    std::string_view mix_digest;

    uint64_t nonce;
};
using ShareCn = StratumShare<StratumProtocol::CN>;

template <size_t BLOCK_HEADER_SIZE>
struct WorkerContext
{
    uint32_t current_height;
    simdjson::ondemand::parser json_parser;
    uint8_t block_header[BLOCK_HEADER_SIZE];

    #if HASH_ALGO == HASH_ALGO_VERUSHASH
    CVerusHashV2 hasher = CVerusHashV2(SOLUTION_VERUSHHASH_V2_2);
    #endif
};

enum class ResCode
{
    OK = 1,
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
struct RpcResult
{
    RpcResult(ResCode e, std::string m = "true") : code(e), msg(m){}

    // static RpcResult Ok = RpcResult(ResCode::OK);

    ResCode code;
    std::string msg;
};
struct ShareResult
{
    ResCode code;
    std::string message;
    double difficulty;
    std::array<uint8_t, 32> hash_bytes;
};

#endif