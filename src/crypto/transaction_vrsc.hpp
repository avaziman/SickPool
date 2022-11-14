// #ifndef transaction_vrsc_HPP_
// #define transaction_vrsc_HPP_
// #include <iomanip>
// #include <iostream>
// #include <tuple>
// #include <vector>

// #include "static_config.hpp"
// #include "transaction.hpp"
// #include "utils.hpp"
// #include "verushash/endian.h"

// class TransactionVrsc : public Transaction
// {
//    public:
//     TransactionVrsc(uint32_t locktime = 0x00000000)
//         : Transaction(locktime)
//     {
//     }

//     void GetBytes(std::vector<uint8_t>& bytes) /*override*/;

//     // since PBAAS_ACTIVATE
//     void AddFeePoolOutput(std::string_view coinbaseHex);

//    private:
//     const uint32_t expiryHeight = 0;
// };

// using transaction_t = TransactionVrsc;
// #endif