/**
 * @file cmdlog.cpp
 * @author https://github.com/atejeda/cmdlog
 * @date September 2016
 * @brief A minimal command line tool to query almasw (ALMA Software) 
 *        elasticsearch log repository. (portable?, who knows).
 *
 * Copyright (C) 2016  https://github.com/atejeda
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 * Notes:
 *
 * check -fno-exceptions and -fno-rtti.
 * https://gcc.gnu.org/wiki/CppConventions
 * https://google.github.io/styleguide/cppguide.html
 *
 * References:
 * 
 * ANSI scape codes (colors)
 *  - https://en.wikipedia.org/wiki/ANSI_escape_code
 *
 * Query references:
 *  - https://www.elastic.co/guide/en/elasticsearch/reference/current/query-dsl-query-string-query.html
 */

// #ifndef __linux__
// #error "Only linux is supported"
// #endif

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

#include <cstdio>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <iterator>
#include <functional>
#include <iomanip>

#include "mongoose.h"
#include "json.h"
#include "linenoise.h"

using namespace std;

// defines

#define GCC_VERSION ((__GNUC__ * 10000) + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#define LOGMSG(arg0, arg1) ((cout << "[ logger ] " << (__FUNCTION__) << " # (" << arg0 << "), " << (arg1) << endl), (void)0)

#define ANSI_COLOR_BLACK        "\033[0;30m"
#define ANSI_COLOR_RED          "\033[0;31m"
#define ANSI_COLOR_GREEN        "\033[0;32m"
#define ANSI_COLOR_YELLOW       "\033[0;33m"
#define ANSI_COLOR_BLUE         "\033[0;34m"
#define ANSI_COLOR_MAGENTA      "\033[0;35m"
#define ANSI_COLOR_CYAN         "\033[0;36m"
#define ANSI_COLOR_WHILE        "\033[0;37m"

#define ANSI_COLOR_BOLD_BLACK   "\033[1;30m"
#define ANSI_COLOR_BOLD_RED     "\033[1;31m"
#define ANSI_COLOR_BOLD_GREEN   "\033[1;32m"
#define ANSI_COLOR_BOLD_YELLOW  "\033[1;33m"
#define ANSI_COLOR_BOLD_BLUE    "\033[1;34m"
#define ANSI_COLOR_BOLD_MAGENTA "\033[1;35m"
#define ANSI_COLOR_BOLD_CYAN    "\033[1;36m"
#define ANSI_COLOR_BOLD_WHITE   "\033[1;37m"

#define ANSI_COLOR_RESET        "\033[0m"

// typedefs

typedef struct {
    std::string name;
    bool value;
} logfield_t;

typedef struct ansicolor {
    string ansicode;

    ansicolor(string code) {
        ansicode = code;
    }

    friend std::ostream& operator<<(ostream& out, const ansicolor& color) { 
        // use c++11 stream iterator over the string
        auto code = &color.ansicode;
        for (unsigned int i = 0; i < code->size(); i++) {
            out.put((*code)[i]);
        }
        return out;
    }
} ansicolor;

// static constants

static const char* header = "Content-Type: application/json; charset=UTF-8\r\n";

static const string esurl = "http://localhost:9200"; //elk-master@osfp9200

static const std::string searchq = R"DL({
    "from" : 0,
    "size" : 1,
    "highlight": {
        "pre_tags": [
            "some_tag"
        ],
        "post_tags": [
            "some_tag"
        ],
        "fields": {
          "*": { }
        },
        "require_field_match": false,
        "fragment_size": 2147483647
    },
    "query": {
        "query_string":  {
            "default_field" : "text",
            "fields" : ["*"],
            "query": "Host:gas06 AND text:fullauto AND tags:AOS64",
            "analyze_wildcard": true
        }
    },
    "sort": [
        { 
            "@timestamp": { 
                "order": "desc" 
            }
        }
    ]   
})DL";

// function pointer to map commands to functions
typedef void (*cmdFunction)(const vector<string>&);
//typedef std::function<void (cmdFunction &)(vector<string>)> cmdFunction;

// ansi colors

static const ansicolor color_black        (ANSI_COLOR_BLACK);
static const ansicolor color_red          (ANSI_COLOR_RED);
static const ansicolor color_green        (ANSI_COLOR_GREEN);
static const ansicolor color_yellow       (ANSI_COLOR_YELLOW);
static const ansicolor color_blue         (ANSI_COLOR_BLUE);
static const ansicolor color_magenta      (ANSI_COLOR_MAGENTA);
static const ansicolor color_cyan         (ANSI_COLOR_CYAN);
static const ansicolor color_white        (ANSI_COLOR_WHILE);

static const ansicolor color_bold_black   (ANSI_COLOR_BOLD_BLACK);
static const ansicolor color_bold_red     (ANSI_COLOR_BOLD_RED);
static const ansicolor color_bold_green   (ANSI_COLOR_BOLD_GREEN);
static const ansicolor color_bold_yellow  (ANSI_COLOR_BOLD_YELLOW);
static const ansicolor color_bold_blue    (ANSI_COLOR_BOLD_BLUE);
static const ansicolor color_bold_magenta (ANSI_COLOR_BOLD_MAGENTA);
static const ansicolor color_bold_cyan    (ANSI_COLOR_BOLD_CYAN);
static const ansicolor color_bold_white   (ANSI_COLOR_BOLD_WHITE);

static const ansicolor color_reset        (ANSI_COLOR_RESET);

// tags used to highlight fields
static const string pretag  = color_red.ansicode;
static const string posttag = color_reset.ansicode;

// map of commands to functions to  
static std::map<string, cmdFunction> cmdFunctions;

// variable to control responses from rest
static int s_exit_flag = 0;

// log fields
static std::map<std::string, logfield_t> logfields = {
    { "tags",         (logfield_t) { "tags",         true } },  // Tags
    { "sourceobject", (logfield_t) { "SourceObject", true  } }, // SourceObject
    { "thread",       (logfield_t) { "Thread",       false } }, // Thread
    { "loglevel",     (logfield_t) { "LogLevel",     true } },  // LogLevel
    { "timestamp",    (logfield_t) { "TimeStamp",    true  } }, // TimeStamp
    { "logid",        (logfield_t) { "LogId",        false } }, // LogId
    { "process",      (logfield_t) { "Process",      false } }, // Process
    { "host",         (logfield_t) { "Host",         false } }, // Host
    { "text",         (logfield_t) { "text",         true  } }, // text
    { "file",         (logfield_t) { "File",         false } }, // File
    { "routine",      (logfield_t) { "Routine",      false } }, // Routine
    { "line",         (logfield_t) { "Line",         false } }, // Line
    { "data",         (logfield_t) { "Data",         false } }  // Data
};

// log fiels in a order to be shown
static vector<logfield_t*> vlogfields = {
    &(logfields["tags"]),
    &(logfields["logid"]),
    &(logfields["timestamp"]),
    &(logfields["loglevel"]),
    &(logfields["host"]),
    &(logfields["process"]),
    &(logfields["thread"]),
    &(logfields["sourceobject"]),
    &(logfields["routine"]),
    &(logfields["file"]),
    &(logfields["line"]),
    &(logfields["text"]),
    &(logfields["data"])
};

// variables used in json queries
static std::map<std::string, std::string> variables = {
    { "from", "0" },
    { "size", "50" }
};

// client

string constructUrl(const string& url) {
    string constructed_url = esurl + url;
    return constructed_url;
}

string constructQuery(const string& query, const string& from, const string& size) {

    std::stringstream jsonstream;
    Json::Value json;

    jsonstream << searchq;
    jsonstream >> json;


    json["query"]["query_string"]["query"] = query;
    json["highlight"]["pre_tags"][0] = pretag;
    json["highlight"]["post_tags"][0] = posttag;

    json["from"] = from;
    json["size"] = size;

    return json.toStyledString();
}

void connectionHandler(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message* reponse = (struct http_message*) ev_data;
    
    int connect_status = 0;
    long hits = 0;

    stringstream jsonstream;
    Json::Value json;

    switch (ev) {
        case MG_EV_CONNECT:

            connect_status = *(int*) ev_data;

            if (connect_status != 0) {
                cout << "Error during the connection...: "<< strerror(connect_status);
                nc->flags |= MG_F_SEND_AND_CLOSE;
                s_exit_flag = 1;
            }

            break;

        case MG_EV_HTTP_REPLY:
            // printf("debug: \n%.*s\n", (int) reponse->body.len, reponse->body.p);          

            jsonstream << reponse->body.p;
            jsonstream >> json;

            // cout << color_yellow << json.toStyledString() << color_reset << endl;

            // check if is possible to free reponse->body.p

            hits = json["hits"]["hits"].size();

            if (!hits) {
                auto errors = &json["error"]["root_cause"];

                for (int i = 0; i < errors->size(); i++) {
                    auto reason = (*errors)[i]["reason"].asString();
                    auto type = (*errors)[i]["type"].asString();

                    cout << color_red << "error: " << color_reset << type << ", " << reason << endl;
                }

                s_exit_flag = 1;
                break;
            } 

            for (int i = hits - 1; i >= 0 ; i--) {

                Json::Value highlights = json["hits"]["hits"][i]["highlight"];

                for (int f = 0; f < vlogfields.size(); f++) {
                    try {
                        auto vfield = vlogfields[f];
                        string* name = &((*vfield).name);
                        bool* value = &((*vfield).value);

                        Json::Value highlight_field = highlights[*name];
                        bool is_highlighted = highlight_field.size() == 1;

                        if (*value) {
                            auto field = &(json["hits"]["hits"][i]["_source"][*name]);
                            auto strfield = (*field).asString();

                            if (strfield[strfield.size() - 1] == '\n') {
                                strfield = strfield.substr(0, strfield.size() - 2);
                            }
                            
                            if (*name == "tags") {
                                // manage the array, print just first tag element
                                cout << (*field)[0].asString() << " ";
                            }

                            // log level in colors
                            if (*name == "LogLevel") {
                                cout.width(9); 
                                cout << left;

                                if (strfield == "Error" || strfield == "Emergency") {
                                    cout << color_red;
                                } else if (strfield == "Warning") {
                                    cout << color_yellow; 
                                } else if (strfield == "Info") {
                                    cout << color_green; 
                                } else if (strfield == "Debug") {
                                    cout << color_magenta; 
                                }

                                cout << (*field).asString() << color_reset << left << " ";
                            } else {
                                if (is_highlighted) {// string::npos
                                    // use the hightlihted one instead
                                    field = &highlight_field[0];
                                    string field_string = (*field).asString();
                                    cout << field_string << " ";
                                } else {
                                    cout << (*field).asString() << " ";
                                }
                            }
                        }
                    } catch (...) {
                        // unparseable, do nothing (?)...
                    }
                }

                cout << endl;
            }

            nc->flags |= MG_F_SEND_AND_CLOSE;
            s_exit_flag = 1;
            break;

        default:
            break;
    }
}

void cmdQuery(const vector<std::string>& input) {
    if (input.size() < 2) {
        std::cout << "Usage: " << input[0] << " <query>\n";
        return;
    }

    // data setup

    string query;

    for (unsigned i = 1; i < input.size() - 1; i++) {
        query += input[i] + " ";
    }

    if (input.size() > 1) {
        query += input[input.size() - 1];
    }
    
    auto from = variables["from"];
    auto size = variables["size"];

    auto url = constructUrl("/aos64-*/_search");
    auto jsonquery = constructQuery(query, from, size);

    // api call

    s_exit_flag = 0;

    struct mg_mgr mmanager;
    struct mg_connection *mconnection;

    mg_mgr_init(&mmanager, NULL);

    mconnection = mg_connect_http(&mmanager, connectionHandler, url.c_str(), header, jsonquery.c_str());

    mg_set_protocol_http_websocket(mconnection);

    while (s_exit_flag == 0) {
        mg_mgr_poll(&mmanager, 1000);
    }

    mg_mgr_free(&mmanager);
}

// cmd functions

void cmdShow(const vector<string>& argv) {
    // update this to c++11, auto& iter = mapwhatever
    // std::map<std::string, logfield_t>::iter

    cout << "fields" << endl;
    for (auto iter = logfields.begin(); iter != logfields.end(); iter++) {
        cout << " " << iter->second.name;

        for (unsigned int i = 0; i < 14 - iter->first.size(); i++) {
            cout << " ";
        }

        cout << ": " << iter->second.value << endl;
    }

    cout << "variables" << endl;
    for (auto iter = variables.begin(); iter != variables.end(); iter++) {
        cout << " " << iter->first << " = " << iter->second << endl;
    }
}

void cmdSetv(const vector<string>& argv) {
    if (argv.size() < 3) {
        cout << "usage: " << argv[0] << "set <variable> <value>" << endl;
        return;
    }

    variables[argv[1]] = argv[2];
}

void cmdManageField(string& field, const bool& enable = false) {
    std::transform(field.begin(), field.end(), field.begin(), ::tolower);

    if (logfields.find(field) == logfields.end()) {
        cout << field << " unvalid field name" << endl;
    } else {
        logfields[field].value = enable;
    }
}

void cmdEnableField(const vector<string>& argv) {
    string field = argv[1];
    cmdManageField(field, true);
}

void cmdDisableField(const vector<string>& argv) {
    string field = argv[1];
    cmdManageField(field, false);
}

void cmdTest(const vector<std::string>& input) {
    string query;

    for (unsigned i = 1; i < input.size() - 1; i++) {
        query += input[i] + " ";
    }

    if (input.size() > 1) {
        query += input[input.size() - 1];
    }
    
    auto from = variables["from"];
    auto size = variables["size"];
    auto lte = variables["lte"];
    auto gte = variables["gte"];

    auto url = constructUrl("/aos64-*/_search");
    auto jsonquery = constructQuery(query, from, size);

    cout << jsonquery << endl;
}

// functions

void processInput(const string& userinput) {
    if (!userinput.size() || userinput[0] == '#') {
        return;
    }

    istringstream inputstream(userinput);

    vector<string> arguments;
    string argument;
    while (inputstream >> argument) {
        arguments.push_back(argument);
    }

    string* commandline = &arguments[0];

    std::map<std::string, cmdFunction>::iterator iter;

    if ((iter = cmdFunctions.find(*commandline)) == cmdFunctions.end()) {
        cout << "command doesn't exists" << endl;
    } else {
        cmdFunction mappedFunction = iter->second;
        (*mappedFunction)(arguments);
    }
}

string getLineHistory() {
    string home(getpwuid(getuid())->pw_dir);
    string history(home + "/.cmdlog_history");
    return history;
}

// main

int main(int argc, char* argv[], char* envp[]) {
    // register function maps
    cmdFunctions["show"] = cmdShow;
    cmdFunctions["set"] = cmdSetv;
    cmdFunctions["enable"] = cmdEnableField;
    cmdFunctions["disable"] = cmdDisableField;
    cmdFunctions["test"] = cmdTest;
    cmdFunctions["query"] = cmdQuery;

    string lineHistory = getLineHistory();
    linenoiseHistoryLoad(lineHistory.c_str());

    char* line;

    if (argc > 1) {
        line = argv[1];
        linenoiseHistoryAdd(line);
        linenoiseHistorySave(lineHistory.c_str());
        processInput(string(line));

        return EXIT_SUCCESS;
    }

    cout << "> history located at " << lineHistory << endl;
    while((line = linenoise("> ")) != NULL) {
        linenoiseHistoryAdd(line);
        linenoiseHistorySave(lineHistory.c_str());
        processInput(string(line));
    }

    return EXIT_SUCCESS;
}
