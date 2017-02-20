[![Build Status](https://travis-ci.org/atejeda/cmdlog.svg?branch=master)](https://travis-ci.org/atejeda/cmdlog)

# cmdlog

An interactive and small command line tool to query almasw (ALMA Software) elasticsearch log repository.

Even though kibana is useful to perform analysis, it doesn't help too much for the developer debug problems (at least is a problematic thing to me), this is due how the logs are presented to the user, the idea of this tool is to query elasticsearch directly without passing through kibana and print out the logs in plain ASCII (similar to the container logs) to the stdout allowing the possibility to copy them.

## Features

 - It's a terminal based tool, supports ASCII scape codes and history is saved in $HOME/.cmdlog_history

## Build

Third party software is retrived automatically and doesn't depends on libraries or other tools, this also applies to this tool as well, run ./build.sh to retrieve and build the application.

Depends on your compiler version you may want to change the g++ std flag to one of these:

- -std=c++11
- -std=c++0x
- -std=gnu++11

The tool compiles in almost all modern gnu/linux distros, including RHEL6, clang not supported yet.

## License

- https://github.com/atejeda/cmdlog/blob/master/LICENSE

Thirdparty software used:

- https://github.com/antirez/linenoise/blob/master/LICENSE
- https://github.com/cesanta/mongoose/blob/master/LICENSE
- https://github.com/open-source-parsers/jsoncpp/blob/master/LICENSE
