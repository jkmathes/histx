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
```shell
save_cmd() {
    path/to/histx index $(history 1 | awk '{$1=""; print $0}')
}

export PROMPT_COMMAND=save_cmd
```

For zsh, set your `pre_cmd()` hook:
```shell
    path/to/histx index $(history -1 | awk '{$1=""; print $0}')
}
```

## Importing current history

At present, you can import your current history using the `index-in` command:

```shell
# for zsh
history 1 | path/to/histx index-in

# for bash
history | path/to/histx index-in
```

## Usage
```
histx find [input tokens]
```
