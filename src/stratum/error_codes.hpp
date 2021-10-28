#ifndef STRATUM_ERROR_CODES_HPP_
#define STRATUM_ERROR_CODES_HPP_

// official error codes from https://braiins.com/stratum-v1/docs

enum class ErrorCodes
{
    UNKNOWN = 20,
    JOB_NOT_FOUND = 21, /* stale */
    DUPLICATE_SHARE = 22,
    LOW_DIFFICULTY_SHARE = 23,
    UNAUTHORIZED_WORKER = 24,
    NOT_SUBSCRIBED = 25
};

#endif