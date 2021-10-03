# histx
Find anything in your command history

## Dependencies
Just a few - MacOS should be set; for linux, you need sqlite3 and openssl libraries

On Ubuntu, for example:
```
apt-get install -y build-essential libsqlite3-dev libssl-dev
```
## Installation
On MacOS, for example in `.bash_profile`:
```
save_cmd() {
    path/to/histx index $(history 1 | awk '{$1=""; print $0}')
}

export PROMPT_COMMAND=save_cmd
```

## Usage
```
histx find [input tokens]
```
