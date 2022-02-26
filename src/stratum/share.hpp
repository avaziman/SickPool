#ifndef SHARE_HPP_
#define SHARE_HPP_

struct Share{
    const char* worker;
    const char* jobId;
    const char* time;
    const char* nonce2;
    const char* solution;
    int solutionSize;
};

#endif