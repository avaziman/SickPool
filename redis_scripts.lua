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
-- hgetall takes the most time
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
                redis.call("HGETALL", "block:"..index)
            )

        end

        return result
    end
)