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

-- key 1  -> index key
-- key 2  -> key prefix
-- ARGV -> field used in ZRANGE
-- O(N), n = number of keys
redis.register_function(
    "getblocksbyindex",
    function(KEYS, ARGV)
        local indexes = redis.call("ZRANGE", KEYS[1], unpack(ARGV))
        local totalResults = redis.call("ZCARD", KEYS[1])

        local result = {totalResults, {}}
        for _, index in ipairs(indexes) 
        do
            table.insert(
                result[2],
                redis.call("GET", KEYS[2]..index)
            )

        end

        return result
    end
)

-- key 1  -> index key
-- key 2  -> key prefix
-- ARGV -> field used in ZRANGE
-- O(N), n = number of keys
redis.register_function(
    "getsolversbyindex",
    function(KEYS, ARGV)
        local indexes = redis.call("ZRANGE", KEYS[1], unpack(ARGV))
        local totalResults = redis.call("ZCARD", KEYS[1])

        local result = {totalResults, {}}
        for _, index in ipairs(indexes) 
        do
                
            local solver = {index, unpack(redis.call("HMGET", KEYS[2]..index, "hashrate", "mature-balance", "join-time", "worker-count"))}
            table.insert(
                result[2],
                solver
            )

        end

        return result
    end
)