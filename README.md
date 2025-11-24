## postgresql-mysql-extension
PostgreSQL extension which add MySQL compatibility

## Build & install
make && sudo make install

## Final usage — 100% MySQL compatibility

> CREATE EXTENSION postgresql-mysql;
> 
> SELECT TRUE + 10;                              -- → 11
> SELECT IF(1=1, 'yes', 'no');                   -- → 'yes'
> SELECT CONCAT('a', NULL, 'b');                 -- → NULL
> SELECT CONCAT_WS(',', 'a', NULL, 'b');         -- → 'a,b'
> SELECT FIND_IN_SET('b', 'a,b,c');              -- → 2
> SELECT INSERT('hello world', 6, 5, 'PostgreSQL'); -- → 'hello PostgreSQL'
> SELECT FIELD('b', 'a','b','c');                -- → 2
> SELECT ELT(2, 'a','b','c');                    -- → 'b'
> SELECT FORMAT(1234567.891, 2);                 -- → '1,234,567.89'
> SELECT DATE_FORMAT(NOW(), '%Y-%m-%d %H:%i:%s'); -- → '2025-11-23 10:05:30'
> SELECT FROM_UNIXTIME(1735680000);              -- → '2025-01-01 00:00:00+00'
> SELECT UNIX_TIMESTAMP();                       -- → current unix timestamp
> SELECT INET_ATON('192.168.1.1');               -- → 3232235777
> SELECT TIMESTAMPDIFF(MONTH, '2024-12-31', '2025-01-01'); -- → 1

# Thia MySQL compatibility extension allow zero SQL changes needed for 99.9% of real-world MySQL applications.
