CREATE TABLE addresses (
    id INT UNIQUE AUTO_INCREMENT,
    address char(255) UNIQUE
);

CREATE TABLE workers (
    id INT UNIQUE AUTO_INCREMENT;

miner_id INT;

FOREIGN KEY (miner_id) REFERENCES addresses(id)
);

-- CREATE PROCEDURE 
CREATE TABLE blocks (
    id INT UNIQUE AUTO_INCREMENT,
    status TINYINT,
    worker_id INT,
    hash char(64),
    reward BIGINT UNSIGNED,
    time_ms BIGINT UNSIGNED,
    duration_ms BIGINT UNSIGNED,
    height INT UNSIGNED,
    difficulty DOUBLE,
    effort_percent DOUBLE,
    PRIMARY KEY (id),
    INDEX (
        miner_id,
        worker_id,
        reward,
        duration_ms,
        difficulty,
        effort_percent
    ),
    FOREIGN KEY (worker_id) REFERENCES workers(id)
);


DELIMITER //
CREATE PROCEDURE AddBlock (
    IN worker_id INT,
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

CREATE PROCEDURE xx() BEGIN
SELECT
    blocks.miner_id,
    addresses.address
FROM
    blocks
    JOIN addresses ON blocks.miner_id = addresses.id;

END;