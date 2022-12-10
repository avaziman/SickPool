#include "stratum_notifier_lib/stratum_notifier_lib.hpp"

// stratum.block_notify
// stratum.wallet_notify
int main(int argc, char* argv[]) { return stratum_notify(argv[1], argv[2]); }