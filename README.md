[![Build Status](https://travis-ci.org/atejeda/cmdlog.svg?branch=master)](https://travis-ci.org/atejeda/cmdlog)

# cmdlog

An interactive and small command line tool to query almasw (ALMA Software) elasticsearch log repository.

Even though kibana is useful to perform analysis, it doesn't help too much for the developer debug problems (at least is a problematic thing to me), this is due how the logs are presented to the user, the idea of this tool is to query elasticsearch directly without passing through kibana and print out the logs in plain ASCII (similar to the container logs) to the stdout allowing the possibility to copy them.

The tool also allow to choose which log fields should be printed out to the stdout, range date and a size limit (how many entries), this is possible by setting few variables, e.g.:

```
cmdlog % enable data
cmdlog % set size 300
cmdlog % show
fields
 Data          : 1
 File          : 0
 Host          : 0
 Line          : 0
 LogId         : 0
 LogLevel      : 0
 Process       : 0
 Routine       : 0
 SourceObject  : 1
 tags          : 1
 text          : 1
 Thread        : 0
 TimeStamp     : 1
variables
 from = 0
 gte = 2016-11-14T13:39:49.179
 lte = 2016-11-15T13:39:49.179
 size = 300
 restricted = 1
```

## variables

Variables are just variables used in the search json query:

 - *from*, an offset for the results, *0* means from the start.
 - *size*, how many log entries the response can have
 - *gte*, lower limit for a date range, e.g.: from this specific date...
 - *lte*, lower limit for a date range, e.g.: to this specific date, date format like 2016-11-14T13:39:49.179
 - *restricted*, if *1* is set, the query will use *gte* and *lte*, date format like 2016-11-14T13:39:49.179

 ```
 set <variable> value
 ```

 ## logfields

 Enable and disable logfields to be printed out.

 ```
 enable <logfield name>
 ```

 ```
 disable <logfield name>
 ```

 ## other commands
 ```
 show
 ```

## Other features

 - It's a terminal based tool, supports ASCII scape codes and history is saved in $HOME/.cmdlog_history

## Build

Third party software is retrived automatically and doesn't depends on libraries or other tools, this also applies to this tool as well.

```
make clean all
```

Depends on your compiler version you may want to change the g++ std flag to one of these:

- -std=c++11
- -std=c++0x
- -std=gnu++11

The tool compiles in almost all modern gnu/linux distros, including RHEL6.

## Bugs?

To lazy to fork and pull request?, :).

## Roadmap

Mostly improvements:

- implement autocomplete
- load query directly from the commandline without open the terminal
- load pre-saved query
- load custom library developed by the user to analize the logs, e.g.: state machine transitions.

## License

Thirdparty software used:

- https://github.com/antirez/linenoise/blob/master/LICENSE
- https://github.com/cesanta/mongoose/blob/master/LICENSE
- https://github.com/open-source-parsers/jsoncpp/blob/master/LICENSE

Everything else:

<p align="center">
<a href="https://www.gnu.org/licenses/gpl-3.0.txt" class="rich-diff-level-one"><img alt="GPLv3" src="https://www.gnu.org/graphics/gplv3-127x51.png" style="max-width:100%;"></a>
</p>

```
A minimal command line tool to query almasw (ALMA Software) elasticsearch log repository.
Copyright (C) 2016  https://github.com/atejeda

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
```
