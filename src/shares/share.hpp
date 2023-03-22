#ifndef SHARE_HPP_
#define SHARE_HPP_
#include <simdjson.h>
#include <ranges>
#include <array>
#include <charconv>
#include <memory>
#include <string_view>
#include <vector>

#include "config_vrsc.hpp"
#include "constants.hpp"
#include "static_config.hpp"
template <StratumProtocol sp>
struct StratumShareT
{
};

template <>
struct StratumShareT<StratumProtocol::ZEC> : public CoinConstantsZec,
                                             public StratumConstants
{
    // as in order of appearanceז
    std::string_view worker;
    std::string_view job_id;
    // unhexlify nonce2 and solution directly into block header
    std::string_view nonce2_sv;
    std::string_view solution;

    uint32_t time;
};
using ShareZec = StratumShareT<StratumProtocol::ZEC>;

template <>
struct StratumShareT<StratumProtocol::BTC>
{
    // as in order of appearance
    std::string_view worker;
    std::string_view job_id;
    std::string_view extranonce2;
    std::string_view time_sv;
    // std::string_view nonce_sv;

    uint32_t time;
    uint32_t nonce;
};
using ShareBtc = StratumShareT<StratumProtocol::BTC>;

template <>
struct StratumShareT<StratumProtocol::CN>
{
    std::string_view worker;

    // std::string_view nonce_sv;
    // used to identify job instead of JobId.
    std::string_view job_id;  // header_pow
    std::string_view mix_digest;

    uint64_t nonce;
};
using ShareCn = StratumShareT<StratumProtocol::CN>;

template <size_t BLOCK_HEADER_SIZE>
struct WorkerContext
{
    uint32_t current_height;
    simdjson::ondemand::parser json_parser;
    std::array<uint8_t, BLOCK_HEADER_SIZE> block_header;

    CVerusHashV2 hasher = CVerusHashV2(SOLUTION_VERUSHHASH_V2_2);
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
    RpcResult(ResCode e, std::string m = "true") : code(e), msg(m) {}

    // static RpcResult Ok = RpcResult(ResCode::OK);

    ResCode code;
    std::string msg;
};
struct ShareResult
{
    ResCode code;
    std::string message;
    double difficulty = 0.0;
    std::array<uint8_t, 32> hash_bytes;
};

#endif