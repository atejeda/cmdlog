/**
 * A minimal command line tool to query almasw (ALMA Software) elasticsearch log repository.
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
 * - Indexed patterns aos64-* (default), aos-*
 * - Not added but could be enabled in options:
 * { "analyze_wildcard": true }
 * { "unmapped_type": "boolean" }
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

#define ANSI_COLOR_WHITE_BOLD "\033[1;37m"
#define ANSI_COLOR_RED        "\033[31m"
#define ANSI_COLOR_YELLOW     "\033[33m"
#define ANSI_COLOR_NONE       "\033[0m"

// constants

static const char* header = "Content-Type: application/json; charset=UTF-8\r\n";

static const std::string esurl = "http://localhost:9200"; //elk-master@osfp9200

static const std::string searchq = R"DL({
    "from" : 0,
    "size" : 1,
    "highlight": {
        "pre_tags": [
            "RRRRRRRRRR"
        ],
        "post_tags": [
            "BBBBBBB"
        ],
        "fields": {
          "*": { }
        },
        "require_field_match": false,
        "fragment_size": 2147483647
    },
    "query": {
        "query_string":  {
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

static const std::string searchqt = R"DL({
    "from" : 0,
    "size" : 1,
    "query" : {
        "filtered" : {
            "query" : {
                "query_string": {
                    "default_field" : "text",
                    "query" : "*"
                }
            },
            "filter" : {
                "range" : {
                    "TimeStamp" : {
                        "lte": "2016-11-15T13:39:49.179",
                        "gte": "2016-11-14T13:39:49.179",
                        "format": "strict_date_hour_minute_second_fraction"
                    }
                }
            }
        }
    }
})DL";

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

typedef void (*cmdFunction)(const vector<string>&);
//typedef std::function<void (cmdFunction &)(vector<string>)> cmdFunction;

// static

// ansi colors
static const ansicolor color_red    (ANSI_COLOR_RED);
static const ansicolor color_yellow (ANSI_COLOR_YELLOW);
static const ansicolor color_none   (ANSI_COLOR_NONE);

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
    { "size", "5" },
    { "lte", "2016-11-15T13:39:49.179" },
    { "gte", "2016-11-14T13:39:49.179" },
    { "restricted", "0" }
};

// map of commands to functions to  
static std::map<string, cmdFunction> cmdFunctions;

// variable to control responses
static int s_exit_flag = 0;

// client

string constructUrl(const string& url) {
    string constructed_url = esurl + url;
    return constructed_url;
}

string constructQuery(const string& query, const string& from, 
    const string& size, const string& lte, const string& gte, 
    const bool restricted) {

    std::stringstream jsonstream;
    Json::Value json;

    // decide which query to use
    jsonstream << (restricted ? searchqt : searchq);
    jsonstream >> json;

    // time range
    if (restricted) {
        json["query"]["filtered"]["filter"]["range"]["TimeStamp"]["lte"] = lte;
        json["query"]["filtered"]["filter"]["range"]["TimeStamp"]["gte"] = gte;
        json["query"]["filtered"]["query"]["query_string"]["query"] = query;
    } else {
        json["query"]["query_string"]["query"] = query;
    }

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

            // cout << color_yellow << json.toStyledString() << color_none << endl;

            // check if is possible to free reponse->body.p

            hits = json["hits"]["hits"].size();

            if (!hits) {
                auto errors = &json["error"]["root_cause"];

                for (int i = 0; i < errors->size(); i++) {
                    auto reason = (*errors)[i]["reason"].asString();
                    auto type = (*errors)[i]["type"].asString();

                    cout << color_red << "error: " << color_none << type << ", " << reason << endl;
                }

                s_exit_flag = 1;
                break;
            } 

            // cout << "total hits " << hits << endl;

            for (int i = 0; i < hits; i++) {

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
                                }

                                cout << (*field).asString() << color_none << left << " ";
                            } else {
                                if (is_highlighted) {
                                    // use the hightlihted one instead
                                    field = &highlight_field[0];
                                    
                                    string field_string = (*field).asString();

                                    size_t cposstart = field_string.find("RRRRRRRRRR");
                                    field_string.replace(cposstart, 10, ANSI_COLOR_WHITE_BOLD);

                                    size_t cposend = field_string.find("BBBBBBB");
                                    field_string.replace(cposend, 7, ANSI_COLOR_NONE);
                                    
                                    cout << field_string << " ";
                                } else {
                                    cout << (*field).asString() << " ";
                                }
                            }
                        }
                    } catch (...) {
                        // unparseable, do nothing...
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

    auto restricted = (variables["restricted"] == "1");
    
    auto from = variables["from"];
    auto size = variables["size"];
    auto lte = variables["lte"];
    auto gte = variables["gte"];

    auto url = constructUrl("/aos64-*/_search");
    auto jsonquery = constructQuery(query, from, size, lte, gte, restricted);

    // api call

    s_exit_flag = 0;

    struct mg_mgr mmanager;
    struct mg_connection *mconnection;

    mg_mgr_init(&mmanager, NULL);

    mconnection = mg_connect_http(&mmanager, 
        connectionHandler, url.c_str(), header, jsonquery.c_str());

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

    auto restricted = (variables["restricted"] == "1");
    
    auto from = variables["from"];
    auto size = variables["size"];
    auto lte = variables["lte"];
    auto gte = variables["gte"];

    auto url = constructUrl("/aos64-*/_search");
    auto jsonquery = constructQuery(query, from, size, lte, gte, restricted);

    cout << jsonquery << endl;
}

// functions

void processInput(const string& userinput) {
    if (!userinput.size() || userinput[0] == '#') {
        return;
    }

    istringstream inputstream(userinput);

    // #if GCC_VERSION > 40707
    //     vector<string> arguments { istream_iterator<string>{streamArgs}, istream_iterator<string>{} };
    // #else
    vector<string> arguments;
    string argument;
    while (inputstream >> argument) {
        arguments.push_back(argument);
    }
    // #endif

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

    // processInput(string(argv[1]));
    // return EXIT_SUCCESS;

    string lineHistory = getLineHistory();
    linenoiseHistoryLoad(lineHistory.c_str());

    cout << "% history located at " << lineHistory << endl;

    char* line;
    while((line = linenoise("cmdlog % ")) != NULL) {
        linenoiseHistoryAdd(line);
        linenoiseHistorySave(lineHistory.c_str());
        processInput(string(line));
    }

    return EXIT_SUCCESS;
}