#include "stratum_server.hpp"

StratumServer::StratumServer(CoinConfig cnfg) : coinConfig(cnfg)
{
    for (int i = 0; i < 4; i++)
    {
        if (cnfg.rpcs[i].host.size() != 0)
        {
            SockAddr rpc_addr(cnfg.rpcs[i].host);
            this->rpcs.push_back(
                new DaemonRpc(rpc_addr.ip, rpc_addr.port, cnfg.rpcs[i].auth));
        }
    }

    sockfd =
        socket(AF_INET,      // adress family: ipv4
               SOCK_STREAM,  // socket type: socket stream, reliable byte
                             // connection-based byte stream
               IPPROTO_TCP   // protocol: transmission control protocol (tcp)
        );

    if (sockfd == /*INVALID_SOCKET*/ -1) throw -1;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(cnfg.stratum_port);

    if (bind(sockfd, (const sockaddr *)&addr, sizeof(addr)) != 0) throw -1;

    // init hash functions if needed
    if (coinConfig.algo == "verushash") CVerusHashV2::init();
}

void StratumServer::StartListening()
{
    if (listen(this->sockfd, 1024) != 0)
        throw std::runtime_error(
            "Stratum server failed to enter listenning state.");

    HandleBlockUpdate(Value().SetNull());

    std::thread(&StratumServer::Listen, this).join();
}

void StratumServer::Listen()
{
    std::cout << "Started listenning..." << std::endl;

    while (true)
    {
        int addr_len;
        int conn_fd;
        sockaddr_in conn_addr;
        conn_fd = accept(sockfd, (sockaddr *)&addr, (socklen_t *)&addr_len);
        if (conn_fd <= 0)
        {
            std::cerr << "Invalid connecting socket accepted. Skipping..."
                      << std::endl;
            continue;
        };

        std::thread(&StratumServer::HandleSocket, this, conn_fd).detach();
    }
}

void StratumServer::HandleSocket(int conn_fd)
{
    StratumClient *client =
        new StratumClient(conn_fd, time(NULL), this->coinConfig.default_diff);
    this->clients.push_back(client);

    while (true)
    {
        char buffer[1024 * 4];
        int total = 0;
        int res = 0;
        do
        {
            res = recv(conn_fd, buffer + total, sizeof(buffer) - total, 0);
            if (res > 0)
                total += res;
            else
                break;
        } while (buffer[total - 1] != '\n');

        if (res == 0)
        {
            // clients.erase(client);//TODO: remove client
            break;  // miner disconnected
        }
        else if (res == /*SOCKET_ERROR*/ -1)
            break;

        buffer[total] = '\0';
        this->HandleReq(client, buffer);
    }
}

void StratumServer::HandleReq(StratumClient *cli, char buffer[])
{
    // std::cout << buffer << std::endl;

    Document req(kObjectType);
    req.Parse(buffer);

    int id = 0;
    if (req.HasMember("id") && req["id"].IsInt()) id = req["id"].GetInt();

    std::string method = req["method"].GetString();
    Value params = req["params"].GetArray();

    if (method == "mining.subscribe")
        HandleSubscribe(cli, id, params);
    else if (method == "mining.authorize")
        HandleAuthorize(cli, id, params);
    else if (method == "mining.update_block")
        HandleBlockUpdate(params);
    else if (method == "mining.submit")
        HandleSubmit(cli, id, params);
}

void StratumServer::HandleBlockUpdate(Value &params)
{
    char* buffer;
    try
    {
        buffer = SendRpcReq(1, "getblocktemplate", "");
    }
    catch (const std::exception &e)
    {
        std::cerr << "BLOCK UPDATE RPC ERROR: " << e.what() << std::endl;
        return HandleBlockUpdate(params);
    }

    Document blockT(kObjectType);
    blockT.Parse(buffer);

    std::cout << buffer << std::endl;

    auto txs = blockT["result"]["transactions"].GetArray();
    uint32_t height = blockT["result"]["height"].GetInt();
    int64_t coinbaseValue =
        blockT["result"]["coinbasetxn"]["coinbasevalue"].GetInt64();
    uint32_t curtime = blockT["result"]["curtime"].GetInt();

    std::string heightHex = ToHex(height);

    Block *block;
    BlockHeader *bh;

    if (coinConfig.name == "verus")
    {
        bh = new VerusBlockHeader(
            blockT["result"]["version"].GetInt(),
            ReverseHex(blockT["result"]["previousblockhash"].GetString()),
            curtime, ReverseHex(blockT["result"]["bits"].GetString()),
            ReverseHex(blockT["result"]["finalsaplingroothash"].GetString()),
            blockT["result"]["solution"].GetString());
        block = new Block(bh, HashWrapper::SHA256);
    }

    VerusTransaction coinbaseTx(4, curtime, true, "892f2085");
    coinbaseTx.AddInput(std::string(64, '0'), UINT32_MAX, "03" + heightHex,
                        UINT32_MAX);
    coinbaseTx.AddOutput("RSickPooLhHZ3zRBzhFVsXYiGN18BF1Wh6", coinbaseValue);
    coinbaseTx.AddTestnetCoinbaseOutput();

    std::cout << "coinbase hex: " << coinbaseTx.GetHex() << std::endl;

    // block->AddTransaction(blockT["result"]["coinbasetxn"]["data"].GetString());
    block->AddTransaction(coinbaseTx.GetHex());
    // for (Value::ConstValueIterator itr = txs.begin(); itr != txs.end();
    // itr++)
    //     block->AddTransaction((*itr)["data"].GetString());

    uint256 target(blockT["result"]["target"].GetString());
    std::cout << "difficulty target: " << target.GetHex() << std::endl;

    Job *job = new VerusJob(*block, target);
    jobs.push_back(job);
    this->BroadcastJob(job);

    std::cout << "BLOCK_UPDATE: " << height << std::endl;
}

void StratumServer::HandleSubscribe(StratumClient *cli, int id, Value &params)
{
    // Mining software info format: "SickMiner/6.9"
    if (params[0].IsString()) std::string software_info = params[0].GetString();
    // if (params[1].IsString())
    //     std::string last_session_id = params[1].GetString();
    // if (params[2].IsString()) std::string host = params[2].GetString();
    // if (params[3].IsInt()) int port = params[3].GetInt();

    Document res(kObjectType);
    res.AddMember("id", id, res.GetAllocator());

    Value result(kArrayType);

    result.PushBack(Value().SetNull(), res.GetAllocator());  // SESSION_ID
    result.PushBack(StringRef(cli->GetSubId().c_str()),
                    res.GetAllocator());  // NONCE1 AKA extraNonce1

    res.AddMember("result", result, res.GetAllocator());
    res.AddMember("error", Value().SetNull(), res.GetAllocator());

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    res.Accept(writer);
    std::string response = buffer.GetString();
    response += "\n";

    std::cout << response << std::endl;
    send(cli->GetSock(), response.c_str(), response.size(), 0);
    this->UpdateDifficulty(cli);
    this->BroadcastJob(cli, jobs.back());
}

void StratumServer::HandleAuthorize(StratumClient *cli, int id, Value &params)
{
    // const char *worker_name = params[0].GetString();
    // const char *worker_pass = params[1].GetString();
    // const char *session_id s= req["params"][2].GetString();
    char response[1024];
    int len = snprintf(response, sizeof(response),
                       "{\"id\": %i, \"result\": %s, \"error\": %s}\n", id,
                       "true", "null");

    send(cli->GetSock(), response, len, 0);
}

// https://zips.z.cash/zip-0301#mining-submit
void StratumServer::HandleSubmit(StratumClient *cli, int id, Value &params)
{
    Share share;
    share.worker = params[0].GetString();
    share.jobId = params[1].GetString();
    share.time = params[2].GetString();
    share.nonce2 = params[3].GetString();
    share.solution = params[4].GetString();

    HandleShare(cli, id, share);
}

void StratumServer::HandleShare(StratumClient *cli, int id, Share &share)
{
    VerusJob *job = nullptr;

    for (auto it = jobs.begin(); it != jobs.end(); it++)
    {
        if (share.jobId == (*it)->GetId())
        {
            job = (VerusJob *)*it;
            break;
        }
    }

    if (job == nullptr) return RejectShare(cli, id, ShareError::JOB_NOT_FOUND);

    // TODO: check duplicate

    uint256 cliDiff(cli->GetDifficulty());

    std::string nonce = cli->GetSubId() + share.nonce2;
    nonce += share.solution;
    std::string headerHex =
        job->BHeader()->GetHex(share.time, job->GetMerkleRoot(), nonce);

    std::string hash = HashWrapper::VerushashV2b2(Unhexlify(headerHex));

    uint256 hash256(ReverseHex(hash));
    // std::cout << "header hex: " << headerHex << std::endl;
    std::cout << "block hash  : " << hash256.GetHex() << std::endl;
    std::cout << "block target: " << job->GetTargetDiff().GetHex() << std::endl;
    std::cout << "share target: " << cliDiff.GetHex() << std::endl;

    if (hash256 > cliDiff)
        return RejectShare(cli, id, ShareError::LOW_DIFFICULTY_SHARE);
    // else if (hash256 <= job->GetTargetDiff())
    // {
        std::cout << "found block solution!!!" << std::endl;
        SubmitBlock(job, share, headerHex);
    // }

    std::cout << "share accepted" << std::endl;
}

void StratumServer::RejectShare(StratumClient *cli, int id, ShareError error)
{
    std::string errorMessage = "none";
    switch (error)
    {
        case ShareError::UNKNOWN:
            errorMessage = "unknown";
            break;
        case ShareError::JOB_NOT_FOUND:
            errorMessage = "job not found";
            break;
        case ShareError::DUPLICATE_SHARE:
            errorMessage = "duplicate share";
            break;
        case ShareError::LOW_DIFFICULTY_SHARE:
            errorMessage = "low difficulty share";
            break;
        case ShareError::UNAUTHORIZED_WORKER:
            errorMessage = "unauthorized worker";
            break;
        case ShareError::NOT_SUBSCRIBED:
            errorMessage = "not subscribed";
            break;
    }

    char buffer[1024];
    int len = sprintf(
        buffer,
        "{\"id\": %d, \"result\": null, \"error\": [%d, \"%s\", null]}\n", id,
        error, errorMessage.c_str());
    send(cli->GetSock(), buffer, len, 0);

    std::cout << "rejected share: " << errorMessage << std::endl;
}

void StratumServer::SubmitBlock(Job *job, Share &blockShare,
                                std::string headerHex)
{
    std::string hex = job->GetHex(headerHex);

    std::cout << "block hex: " << hex << std::endl;

    char* res = SendRpcReq(1, "submitblock", hex.c_str());

    std::cout << "block submit result: " << res << std::endl;
}

void StratumServer::UpdateDifficulty(StratumClient *cli)
{
    char request[1024];
    int len = snprintf(
        request, sizeof(request),
        "{\"id\": null, \"method\": \"mining.set_target\", \"params\": "
        "[\"%s\"]}\n",
        cli->GetDifficulty().c_str());

    std::cout << "request: " << request << std::endl;

    send(cli->GetSock(), request, len, 0);
}

void StratumServer::BroadcastJob(Job *job)
{
    char message[1024];
    int len = job->GetNotifyMessage(message, sizeof(message));

    std::cout << "broadcasting req: " << message << std::endl;

    std::vector<StratumClient *>::iterator it;
    for (it = clients.begin(); it != clients.end(); it++)
        send((*it)->GetSock(), message, len, 0);
}

void StratumServer::BroadcastJob(StratumClient *cli, Job *job)
{
    char message[1024];
    int len = job->GetNotifyMessage(message, sizeof(message));

    std::cout << "broadcasting solo: " << message << std::endl;

    send(cli->GetSock(), message, len, 0);
}

char* StratumServer::SendRpcReq(int id, const char* method,
                                      const char* params)
{
    for (DaemonRpc *rpc : this->rpcs)
    {
        char* res = rpc->SendRequest(id, method, params);
        if (res == nullptr) break;
        return res;
    }

    throw std::runtime_error("Failed to send rpc request.");
}