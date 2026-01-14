# postgresql-mysql-extension

A PostgreSQL extension that adds MySQL compatibility by implementing popular MySQL-specific functions and behaviors.

This extension allows many MySQL applications to run on PostgreSQL with **minimal or no SQL changes**. It targets real-world compatibility, covering the most commonly used MySQL functions that differ from PostgreSQL's built-in equivalents.

## Features

- Implements key MySQL string, date, and utility functions
- Reproduces MySQL-specific behaviors (e.g., `TRUE + 10 → 11`, `CONCAT()` returning `NULL` on `NULL` input)
- Easy to install as a standard PostgreSQL extension
- Licensed under the permissive MIT License

### Supported Functions (Examples)

```sql
CREATE EXTENSION "postgresql-mysql";

SELECT TRUE + 10;                              -- → 11 (MySQL boolean arithmetic)
SELECT IF(1=1, 'yes', 'no');                   -- → 'yes'
SELECT CONCAT('a', NULL, 'b');                 -- → NULL
SELECT CONCAT_WS(',', 'a', NULL, 'b');         -- → 'a,b'
SELECT FIND_IN_SET('b', 'a,b,c');              -- → 2
SELECT INSERT('hello world', 6, 5, 'PostgreSQL'); -- → 'hello PostgreSQL'
SELECT FIELD('b', 'a','b','c');                -- → 2
SELECT ELT(2, 'a','b','c');                    -- → 'b'
SELECT FORMAT(1234567.891, 2);                 -- → '1,234,567.89'
SELECT DATE_FORMAT(NOW(), '%Y-%m-%d %H:%i:%s'); -- → e.g., '2026-01-14 11:05:30'
SELECT FROM_UNIXTIME(1735680000);              -- → '2025-01-01 00:00:00+00'
SELECT UNIX_TIMESTAMP();                       -- → current Unix timestamp (integer)
SELECT INET_ATON('192.168.1.1');               -- → 3232235777
SELECT TIMESTAMPDIFF(MONTH, '2024-12-31', '2025-01-01'); -- → 1
```

> Note: This extension aims for 99.9% compatibility with real-world MySQL applications by focusing on the most frequently used differences. It does not implement every MySQL function or edge-case behavior.

## Installation
### Prerequisites
- PostgreSQL (tested on recent versions; should work on 12+)
- Standard build tools (`make`, `gcc`, PostgreSQL development headers)

## Build and Install
```bash
git clone https://github.com/wieslawgolec/postgresql-mysql-extension.git
cd postgresql-mysql-extension
make
sudo make install
```
## Enable the Extension in PostgreSQL
Connect to your database and run:
```sql
CREATE EXTENSION "postgresql-mysql";
```
> The extension name is `postgresql-mysql` (quoted if your PostgreSQL configuration is case-sensitive).

## Testing
After installation, run the example queries above to verify functionality.

## Contributing
Contributions are welcome! Feel free to:

Open issues for bugs or missing functions
Submit pull requests with new MySQL-compatible functions
Improve documentation or add tests

Please follow standard GitHub flow: fork → branch → PR.

## License
This project is licensed under the **MIT License** - see the LICENSE file for details.

## Acknowledgments
Inspired by the need to migrate MySQL-based applications to PostgreSQL without rewriting SQL code.

Thank you for using postgresql-mysql-extension! If you find it useful, consider starring the repository ⭐
