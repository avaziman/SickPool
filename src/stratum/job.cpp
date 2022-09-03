#include "job.hpp"

Job::Job(uint32_t id, Block block) : Block(block), job_id(id) {
    this->target_diff = DifficultyFromBits(FromHex(block.Header()->GetBits()));
    block.CalcMerkleRoot();
}

std::string Job::GetId() {
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << job_id;
    return ss.str();
}
bool Job::ShouldCleanJobs() { return clean_jobs; }
