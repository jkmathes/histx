# histx
Find anything in your command history

## Installation
On MacOS, for example in `.bash_profile`:
```
save_cmd() {
    path/to/histx index $(history 1 | awk '{$1=""; print $0}')
}

export PROMPT_COMMAND=save_cmd
```
