-- #!lua name=sickpool
redis.register_function(
    "geteffortpow",
    function(KEYS)
        local effortArr = redis.call("HGETALL", KEYS[1]..":round_effort_pow")
        local poolHr = unpack(redis.call("TS.GET", "pool_hashrate"))

        local estimatedEffort = tonumber(effortArr[2])
        local totalEffort = tonumber(effortArr[4])
        
        local effort = tostring(totalEffort / estimatedEffort)

        local hashesLeft = estimatedEffort - totalEffort
        local timeLeft = tostring(hashesLeft / poolHr)
        return {effort, timeLeft}
    end
)

-- key 1 -> block index key
-- O(N * M), n = number of blocks, m = number of attribute for block
-- GET takes the most time
redis.register_function(
    "getblocksbyindex",
    function(KEYS, ARGV)
        local indexes = redis.call("ZRANGE", KEYS[1], ARGV[1], ARGV[2])
        local totalResults = redis.call("ZCARD", KEYS[1])

        local result = {totalResults, {}}
        for _, index in ipairs(indexes) 
        do
            table.insert(
                result[2],
                redis.call("GET", "block:"..index)
            )

        end

        return result
    end
)

-- keys[1] = name of the miner index prefix
-- keys[2] = name of the miner index (without prefix)
-- table.unpack in newer versions, (will cause error if table is nil)
redis.register_function(
    "getsolversbyindex",
    function(KEYS, ARGV)
        local indexKey = KEYS[1]..KEYS[2]
        local indexes = redis.call("ZRANGE", indexKey , ARGV[1], ARGV[2], "WITHSCORES")
        local totalResults = redis.call("ZCARD", indexKey)
        local requestedAmount = ARGV[2] - ARGV[1] + 1

        local props = {"hashrate", "balance", "join-time", "worker-count"}
        local propsArrs = {{}, {}, {}, {}}
        local resAmount = math.min(totalResults, requestedAmount)
        
        for j, prop in ipairs(props)
        do
            if prop == KEYS[2]
            then
                for i = 2, resAmount * 2, 2
                do
                    table.insert(propsArrs[j], indexes[i])
                end
            else 
                propsArrs[j] = redis.call("ZMSCORE", KEYS[1]..prop, unpack(indexes))
            end
        end

        local result = {totalResults, {}}
        for i = 1, resAmount, 1
        do
            table.insert(result[2], {indexes[i * 2 - 1]})

            for j, prop in ipairs(props)
            do 
                table.insert(
                    result[2][i],
                    propsArrs[j][i]
                )
            end
        end

        return result
    end
)