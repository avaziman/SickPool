#ifndef SHARE_ERROR_CODES_HPP_
#define SHARE_ERROR_CODES_HPP_

// https://braiins.com/stratum-v1/docs

enum class ShareResult
{
    NONE = 0,
    VALID = 1,       /* I added */
    VALID_BLOCK = 2, /* I added */
    UNKNOWN = 20,
    JOB_NOT_FOUND = 21, /* stale */
    DUPLICATE_SHARE = 22,
    LOW_DIFFICULTY_SHARE = 23,
    UNAUTHORIZED_WORKER = 24,
    NOT_SUBSCRIBED = 25
};

#endif