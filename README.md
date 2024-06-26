![workflow](https://github.com/jkmathes/histx/actions/workflows/c.yml/badge.svg)

# histx
Find anything in your command history

## Dependencies
Just a few - MacOS should be set; for linux, you need sqlite3 and openssl libraries, as well as flex and bison and bsd compatibility

On Ubuntu, for example:
```
apt-get install -y build-essential libsqlite3-dev libssl-dev flex bison libbsd-dev
```
## Installation
On MacOS, for example in `.bash_profile`:
```shell
save_cmd() {
    path/to/histx index $(history 1 | awk '{$1=""; print $0}')
}

export PROMPT_COMMAND="save_cmd;$PROMPT_COMMAND"
```

For zsh, set your `pre_cmd()` hook:
```shell
precmd() {
    path/to/histx index $(history -1 | awk '{$1=""; print $0}')
}
```

If you'd like to run tests:
```shell
make all
./build/histx-test
```

## Importing current history

At present, you can import your current history using the `index` command:

```shell
# for zsh
history 1 | awk '{$1=""; print $0}' | path/to/histx index -

# for bash
history | awk '{$1=""; print $0}' | path/to/histx index -
```

## Reverse Search

### zsh

For zsh, you can create a widget to invoke `histx` for reverse/forward search.
Explore presently does not have any "directionality" notion per se, so bind
as you wish.

Example with zle:
```shell
function _histx-search {
  local VISUAL="$PATH_TO_HISTX explore"
  zle edit-command-line # will invoke $VISUAL with a tmp file name
  zle accept-line
}

zle -N _histx-search # register the widget
bindkey "^R" _histx-search # bind traditional emacs mode rev search 
bindkey -a "/" _histx-search # bind traditional vi mode rev search
```

## Usage
```
usage: histx [-d dbfile] <command>
	options:
		-d path/to/db/file.db -- defaults to $HOME/.histx.db or the value of $HISTX_DB_FILE
		-h this usage information
	commands:
		index             - index all arguments after this command - 
		                    if the only argument after index is `-` read from stdin
		find              - find matching commands using the the passed keywords
		cat               - dump the indexed commands
		explore [tmpfile] - interactive searching of the index
		                    If [tmpfile] is provided, will write the selection (if any)
		                    to the tmp file.
		prune             - allows you to mark entries to be pruned from the index via the explore interface
```

## Settings
Options should be placed in `~/.histx`

Options:
* `explore-basic = true` - simply print the `explore` result instead of writing back into the command buffer
* `search-limit = <limit>` - set the maximum number of results returned in interactive `explore`; default is `5`

