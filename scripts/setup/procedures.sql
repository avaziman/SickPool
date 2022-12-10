CREATE TABLE addresses (
    id INT UNSIGNED UNIQUE AUTO_INCREMENT,
    address char(255) NOT NULL UNIQUE,
    alias char(255) UNIQUE
);

CREATE TABLE miners (
    address_id INT UNSIGNED UNIQUE NOT NULL,
    minimum_payout BIGINT UNSIGNED NOT NULL,

    FOREIGN KEY (address_id) references addresses(id),
    PRIMARY KEY (address_id)
);

CREATE TABLE workers (
    id INT UNSIGNED UNIQUE AUTO_INCREMENT,
    miner_id INT UNSIGNED NOT NULL,
    name CHAR(255) NOT NULL,

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


DELIMITER //
CREATE PROCEDURE AddBlock (
    IN worker_id INT,
    IN miner_id INT,
    IN hash CHAR(64),
    IN reward BIGINT UNSIGNED,
    IN time_ms BIGINT UNSIGNED,
    IN duration_ms BIGINT UNSIGNED,
    IN height INT UNSIGNED,
    IN difficulty DOUBLE,
    IN effort_percent DOUBLE
) BEGIN
    INSERT INTO
        blocks(
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
            0,
            worker_id,
            miner_id,
            hash,
            reward,
            time_ms,
            duration_ms,
            height,
            difficulty,
            effort_percent
        );

END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE AddMiner(IN address CHAR(255), IN alias CHAR(255), IN min_payout BIGINT UNSIGNED)
    BEGIN
        INSERT INTO addresses (address, alias) VALUES (address, alias);
        SET @id = LAST_INSERT_ID();
        INSERT INTO miners (address_id, minimum_payout) VALUES (@id, min_payout);
    END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE GetMiner(IN addr CHAR(255), IN ali CHAR(255))
    BEGIN
        SELECT id from addresses WHERE address = addr OR alias = ali;
    END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE GetWorker(IN minerid INT UNSIGNED, IN wname CHAR(255))
    BEGIN
        SELECT id from workers WHERE miner_id = minerid AND name=wname;
    END//
DELIMITER ;

DELIMITER //
CREATE PROCEDURE AddWorker(IN minerid INT UNSIGNED, IN wname CHAR(255))
    BEGIN
        INSERT INTO workers (miner_id, name) VALUES (minerid, wname);
    END//
DELIMITER ;