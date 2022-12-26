#include "static_config.hpp"
#if SICK_COIN == SIN
#include "job_manager_sin.hpp"

const job_t* JobManagerSin::GetNewJob(const BlockTemplateResSin& rpctemplate)
{
    int64_t additional_fee = 0;
    bool includes_payment = payout_manager->pending_payment.get();

    // inf nodes outputs + dev fee outputs + coinbase reward output
    const std::size_t cb_outputs_count =
        rpctemplate.infinity_nodes.size() + rpctemplate.dev_fee.size() + 1;
    const std::size_t cb_input_count = 1;

    block_template = BlockTemplateBtc(rpctemplate);

    // assume coinbase transaction size + block header is 1000 bytes (extra)
    int block_size = 1000;
    int include_tx_count = 0;

    for (auto tx_res : rpctemplate.transactions)
    {
        const int tx_size = tx_res.data.size();
        
        // wouldn't help because won't update infinity node fees (burn tx fee)
        if (block_size + tx_size >= MAX_BLOCK_SIZE) break;

        block_size += tx_size;

        // we don't receive any tx fee as its burned
        // block_template.coinbase_value += tx_res.fee;
        include_tx_count++;
    }

    //TODO: fix
    block_template.block_size = block_size;

    bool payment_guaranteed =
        includes_payment &&
        block_size + payout_manager->pending_payment->td.data.size() <
            MAX_BLOCK_SIZE;

    // if (payment_guaranteed)
    // {
    //     additional_fee += payout_manager->pending_payment->td.fee;
    // }
    // block_template.coinbase_value += additional_fee;

    // cb
    TransactionBtc cb_tx(cb_input_count, cb_outputs_count);
    const std::size_t coinb1_pos = GetCoinbaseTx(cb_tx, rpctemplate);

    // cbtxd
    TransactionData cb_txd;
    cb_tx.GetBytes(cb_txd.data);

    char cb_data_hex[cb_txd.data.size() * 2];
    Hexlify(cb_data_hex, cb_txd.data.data(), cb_txd.data.size());
    cb_txd.data_hex = std::string_view(cb_data_hex, sizeof(cb_data_hex));
    cb_txd.Hash();

    block_template.tx_list.AddTxData(cb_txd);

    for (int i = 0; i < include_tx_count; i++)
    {
        auto tx_res = rpctemplate.transactions[i];
        TransactionData td(tx_res.data, tx_res.hash);
        block_template.tx_list.AddTxData(td);

        logger.Log<LogType::Info>( 
                    "Included txid: {}, data: {}",
                    std::string_view(td.hash_hex, HASH_SIZE_HEX), td.data_hex);
    }

    if (payment_guaranteed)
    {
        block_template.tx_list.AddTxData(payout_manager->pending_payment->td);
    }


    logger.Log<LogType::Info>( 
                "Coinbase txid: {}, tx: {}", cb_txd.hash_hex, cb_txd.data_hex);

    std::size_t coinb2_pos = coinb1_pos + EXTRANONCE_SIZE + EXTRANONCE2_SIZE;

    block_template.coinb1 = std::span<uint8_t>(cb_txd.data.data(), coinb1_pos);
    block_template.coinb2 = std::span<uint8_t>(cb_txd.data.data() + coinb2_pos,
                                               cb_txd.data.size() - coinb2_pos);

    std::string jobIdHex = fmt::format("{:08x}", job_count);

    auto job =
        std::make_unique<job_t>(jobIdHex, block_template, payment_guaranteed);

    return SetNewJob(std::move(job));
}

const job_t* JobManagerSin::GetNewJob()
{
    BlockTemplateResSin res;
    if (!daemon_manager->GetBlockTemplate(res, jsonParser))
    {
        logger.Log<LogType::Critical>( 
                    "Failed to get block template :(");
        // TODO: make sock err negative maybe http positive to diffrinciate
        return nullptr;
    }

    return GetNewJob(res);
}

std::size_t JobManagerSin::GetCoinbaseTx(TransactionBtc& coinbase_tx,
                                         const BlockTemplateResSin& rpctemplate)
{
    // coinbase output
    std::vector<uint8_t> reward_script;
    Transaction::GetP2PKHScript(pool_addr, reward_script);

    for (auto infnode : rpctemplate.infinity_nodes)
    {
        block_template.coinbase_value -= infnode.value;
    }

    for (auto devfee : rpctemplate.dev_fee)
    {
        block_template.coinbase_value -= devfee.value;
    }

    coinbase_tx.AddOutput(Output(block_template.coinbase_value, reward_script));

    for (auto infnode : rpctemplate.infinity_nodes)
    {
        coinbase_tx.AddOutput(Output(infnode.value, infnode.script_hex));
    }

    for (auto devfee : rpctemplate.dev_fee)
    {
        coinbase_tx.AddOutput(Output(devfee.value, devfee.script_hex));
    }

    std::size_t coinb1_pos =
        AddCoinbaseInput(coinbase_tx, block_template.height);

    return coinb1_pos;
}

std::size_t JobManagerSin::AddCoinbaseInput(TransactionBtc& tx,
                                            const uint32_t height)
{
    unsigned char prevTxIn[HASH_SIZE] = {0};  // null last input
    std::vector<uint8_t> height_script = GenNumScript(height);
    std::vector<uint8_t> signature(1 + height_script.size() +
                                   coinbase_extra.size() + EXTRANONCE_SIZE +
                                   EXTRANONCE2_SIZE);

    signature[0] = height_script.size();
    // 1 = size of height_script ^ it will never exceed 1 byte

    memcpy(signature.data() + 1, height_script.data(), height_script.size());
    memcpy(signature.data() + 1 + height_script.size(), coinbase_extra.data(),
           coinbase_extra.size());
    tx.AddInput(prevTxIn, UINT32_MAX, signature, UINT32_MAX);
    return sizeof(OutPoint) + signature.size() -
           (EXTRANONCE_SIZE + EXTRANONCE2_SIZE);
}

#endif