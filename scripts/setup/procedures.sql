CREATE TABLE addresses (
    id INT UNSIGNED UNIQUE AUTO_INCREMENT,
    address char(255) NOT NULL UNIQUE,
    address_md5 char(32) NOT NULL,
    alias char(255) UNIQUE
);

CREATE TABLE miners (
    address_id INT UNSIGNED UNIQUE NOT NULL,
    mature_balance BIGINT UNSIGNED NOT NULL DEFAULT 0,
    immature_balance BIGINT UNSIGNED NOT NULL DEFAULT 0,
    minimum_payout BIGINT UNSIGNED NOT NULL,
    join_time BIGINT UNSIGNED NOT NULL,
    FOREIGN KEY (address_id) references addresses(id),
    PRIMARY KEY (address_id)
);

CREATE TABLE workers (
    id INT UNSIGNED UNIQUE AUTO_INCREMENT,
    miner_id INT UNSIGNED NOT NULL,
    name CHAR(255) NOT NULL,
    join_time BIGINT UNSIGNED NOT NULL,
    INDEX (miner_id, name),
    FOREIGN KEY (miner_id) REFERENCES miners(address_id)
);

CREATE TABLE blocks (
    id INT UNSIGNED UNIQUE AUTO_INCREMENT,
    status TINYINT NOT NULL,
    miner_id INT UNSIGNED NOT NULL,
    worker_id INT UNSIGNED NOT NULL,
    hash char(64) NOT NULL,
    reward BIGINT UNSIGNED NOT NULL,
    time_ms BIGINT UNSIGNED NOT NULL,
    duration_ms BIGINT UNSIGNED NOT NULL,
    height INT UNSIGNED NOT NULL,
    difficulty DOUBLE NOT NULL,
    effort_percent DOUBLE NOT NULL,
    PRIMARY KEY (id),
    INDEX (
        status,
        miner_id,
        worker_id,
        reward,
        duration_ms,
        difficulty,
        effort_percent
    ),
    FOREIGN KEY (worker_id) REFERENCES workers(id),
    FOREIGN KEY (miner_id) REFERENCES miners(address_id)
);

CREATE TABLE payouts (
    id INT UNSIGNED UNIQUE AUTO_INCREMENT,
    txid CHAR(64) NOT NULL,
    payee_amount INT UNSIGNED NOT NULL,
    paid_amount BIGINT UNSIGNED NOT NULL,
    tx_fee BIGINT UNSIGNED NOT NULL,
    time_ms BIGINT UNSIGNED NOT NULL
);

CREATE TABLE payout_entries (
    payout_id INT UNSIGNED NOT NULL,
    miner_id INT UNSIGNED NOT NULL,
    amount BIGINT UNSIGNED NOT NULL,

    FOREIGN KEY (payout_id) REFERENCES payouts(id),
    FOREIGN KEY (miner_id) REFERENCES miners(address_id)
);

CREATE TABLE payout_stats (
    count INT UNSIGNED NOT NULL DEFAULT 0,
    amount BIGINT UNSIGNED NOT NULL DEFAULT 0,
    next_ms BIGINT UNSIGNED NOT NULL DEFAULT 0
);
INSERT INTO payout_stats () VALUES ();

CREATE TABLE rewards (
    id INT UNSIGNED UNIQUE AUTO_INCREMENT,
    miner_id INT UNSIGNED NOT NULL,
    block_id INT UNSIGNED NOT NULL,
    amount BIGINT UNSIGNED NOT NULL,
    effort DOUBLE NOT NULL,
    INDEX (miner_id),
    FOREIGN KEY (miner_id) REFERENCES miners(address_id),
    FOREIGN KEY (block_id) REFERENCES blocks(id)
);

CREATE TABLE block_stats (
    period CHAR(16) NOT NULL,
    mined INT UNSIGNED NOT NULL DEFAULT 0,
    orphaned INT UNSIGNED NOT NULL DEFAULT 0,
    duration_ms BIGINT UNSIGNED NOT NULL DEFAULT 0,
    effort_percent DOUBLE NOT NULL DEFAULT 0.0
);

INSERT INTO block_stats (period) VALUES ('life');

DELIMITER //
CREATE PROCEDURE AddBlock (
    IN worker_id INT,
    IN miner_id INT,
    IN hash CHAR(64),
    IN reward BIGINT UNSIGNED,
    IN time_ms BIGINT UNSIGNED,
    IN dur_ms BIGINT UNSIGNED,
    IN height INT UNSIGNED,
    IN difficulty DOUBLE,
    IN effort_p DOUBLE
) BEGIN
INSERT INTO blocks
    (
        status,
        worker_id,
        miner_id,
        hash,
        reward,
        time_ms,
        duration_ms,
        height,
        difficulty,
        effort_percent
    )
VALUES
    (
        1,
        worker_id,
        miner_id,
        hash,
        reward,
        time_ms,
        dur_ms,
        height,
        difficulty,
        effort_p
    );

UPDATE block_stats SET 
                 mined=mined+1,
                 duration_ms=duration_ms+dur_ms,
                 effort_percent=effort_percent+effort_p;
                 
END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE AddMiner(
    IN address CHAR(255),
    IN alias CHAR(255),
    IN min_payout BIGINT UNSIGNED,
    IN join_time BIGINT UNSIGNED
) BEGIN
INSERT INTO
    addresses (address, address_md5, alias)
VALUES
    (address, MD5(address), alias);

SET
    @id = LAST_INSERT_ID();

INSERT INTO
    miners (
        address_id,
        mature_balance,
        immature_balance,
        minimum_payout,
        join_time
    )
VALUES
    (@id, 0, 0, min_payout, join_time);

END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE GetMiner(IN addr CHAR(255), IN ali CHAR(255)) BEGIN
SELECT
    id
FROM
    addresses
WHERE
    address = addr
    OR alias = ali;

END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE GetWorker(IN minerid INT UNSIGNED, IN wname CHAR(255)) BEGIN
SELECT
    id
FROM
    workers
WHERE
    miner_id = minerid
    AND name = wname;

END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE AddWorker(
    IN minerid INT UNSIGNED,
    IN wname CHAR(255),
    IN join_time BIGINT UNSIGNED
) BEGIN
INSERT INTO
    workers (miner_id, name, join_time)
VALUES
    (minerid, wname, join_time);
END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE AddReward(
    IN minerid INT UNSIGNED,
    IN blockid INT UNSIGNED,
    IN amount BIGINT UNSIGNED,
    IN effort DOUBLE
) BEGIN
INSERT INTO
    rewards (miner_id, block_id, amount, effort)
VALUES
    (minerid, blockid, amount, effort);

UPDATE
    miners
SET
    immature_balance = immature_balance + amount
WHERE
    address_id = minerid;

END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE MatureRewards(IN blockid INT UNSIGNED, IN status TINYINT) BEGIN
UPDATE
    miners a
    INNER JOIN rewards b ON a.address_id = b.miner_id
SET
    immature_balance = immature_balance - amount,
    mature_balance = mature_balance + IF(status = 2, amount, 0)
WHERE
    block_id = blockid;

IF status=4 THEN
    UPDATE block_stats SET orphaned=orphaned+1;
END IF;

END//
DELIMITER ;


-- DELIMITER //
-- CREATE PROCEDURE GetUnpaidRewards() BEGIN
--     SELECT 
--         address,
--         amount_sum
--     FROM
--     (
--         SELECT
--             addresses.address AS address,
--             amount_sum,
--             miners.minimum_payout AS minimum_payout
--         FROM
--             (
--                 SELECT
--                     rewards.miner_id AS miner_id,
--                     SUM(rewards.amount) AS amount_sum
--                 FROM
--                     rewards
--                 INNER JOIN blocks ON blocks.id = block_id
--                 WHERE status=2
--                 GROUP BY miner_id
--             ) AS temp 
--             INNER JOIN miners ON miners.address_id = miner_id
--             INNER JOIN addresses ON addresses.id = miner_id
--             WHERE amount_sum > minimum_payout
--     ) AS temp2;
-- END//
-- DELIMITER ;

DELIMITER //
CREATE PROCEDURE GetUnpaidRewards(IN minimum BIGINT UNSIGNED) BEGIN
    SELECT address_id, mature_balance, address FROM miners
    INNER JOIN addresses ON addresses.id = miners.address_id
    WHERE mature_balance > IF(minimum_payout > minimum, minimum_payout, minimum);
END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE GetMinerOverview(IN addr CHAR(255)) BEGIN
    SELECT address, alias, mature_balance,immature_balance FROM (
        SELECT
        address,
        alias,
        id
    FROM
        addresses
    WHERE
        LOWER(address)=addr OR alias=addr
    ) AS temp INNER JOIN miners ON miners.address_id = id;
END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE AddPayoutEntry(IN payoutid INT UNSIGNED, IN minerid INT UNSIGNED, IN amount BIGINT UNSIGNED, IN individual_fee BIGINT UNSIGNED) BEGIN
    INSERT INTO payout_entries (payout_id,miner_id, amount) VALUES (payoutid, minerid, amount);
    UPDATE miners SET mature_balance = mature_balance - (amount + individual_fee) WHERE address_id=minerid;
END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE AddPayout(
    IN txid CHAR(255), 
    IN payee_amount INT UNSIGNED,
    IN paid_amount BIGINT UNSIGNED,
    IN tx_fee BIGINT UNSIGNED, 
    IN time_ms BIGINT UNSIGNED) 
BEGIN
    INSERT INTO payouts (txid, payee_amount, paid_amount, tx_fee, time_ms) VALUES (txid, payee_amount, paid_amount, tx_fee, time_ms);
    UPDATE payout_stats SET count=count+1, amount=amount+paid_amount;
END//
DELIMITER ;