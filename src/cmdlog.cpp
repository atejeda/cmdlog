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
 */

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <algorithm>
#include <iterator>
#include <functional>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "mongoose.h"
#include "json.h"
#include "linenoise.h"

using namespace std;

// constants

static const char* header = "Content-Type: application/json; charset=UTF-8\r\n";

//elk-master@osfp9200
static const std::string esurl = "http://localhost:9200";

static const std::string searchq = R"DL(
{
    "from" : 0, 
    "size" : 1,
    "query": {
        "query_string":  {
           "query": "*"
        }    
    }
}
)DL";

static const std::string searchqt = R"DL(
{
    "from" : 0, 
    "size" : 1,
    "query" : {
        "filtered" : {
            "query" : {
                "query_string": {
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
}
)DL";

// defines

#define GCC_VERSION ((__GNUC__ * 10000)  + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

#define LOGMSG(arg0, arg1) ((cout << "[ logger ] " << (__FUNCTION__) << " # (" << arg0 << "), " << (arg1) << endl), (void)0)

// typedefs

typedef struct {
    std::string name;
    bool        value;
} logfield_t;

typedef void (*cmdFunction)(vector<string>);
//typedef std::function<void (cmdFunction &)(vector<string>)> complete_cb;

// static

static std::map<std::string, logfield_t> logfields = {
    { "tags",         (logfield_t) { "tags",         true } },  // Tags
    { "sourceobject", (logfield_t) { "SourceObject", true  } }, // SourceObject
    { "thread",       (logfield_t) { "Thread",       false } }, // Thread
    { "loglevel",     (logfield_t) { "LogLevel",     false } }, // LogLevel
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

static std::map<std::string, std::string> variables = {
    { "from", "0" },
    { "size", "1" },
    { "lte", "2016-11-15T13:39:49.179" },
    { "gte", "2016-11-14T13:39:49.179" },
    { "restricted", "0" }
};

static std::map<std::string, cmdFunction> cmdFunctions;

static int s_exit_flag = 0;

// client

string constructUrl(std::string url) { 
    std::string constructed_url = esurl + url;
    return constructed_url;
}

std::string constructQuery(std::string query, std::string from, std::string size, std::string lte, std::string gte, const bool restricted) {
    std::stringstream ss;
    Json::Value root;

    ss << (restricted ? searchqt : searchq);
    ss >> root;

    // time range
    if (restricted) {    
        root["query"]["filtered"]["filter"]["range"]["TimeStamp"]["lte"] = lte;
        root["query"]["filtered"]["filter"]["range"]["TimeStamp"]["gte"] = gte;
        root["query"]["filtered"]["query"]["query_string"]["query"] = query;
    } else {
        root["query"]["query_string"]["query"] = query;
    }

    root["from"] = from;
    root["size"] = size;
    

    return root.toStyledString();
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *hm = (struct http_message *) ev_data;
    int connect_status;

    stringstream ss;
    Json::Value root;

    switch (ev) {
        case MG_EV_CONNECT:

            connect_status = *(int *) ev_data;
            if (connect_status != 0) {
                cout << "Error connecting to " << esurl << " " << strerror(connect_status);
                s_exit_flag = 1;
            }

            break;

        case MG_EV_HTTP_REPLY:
            // printf("debug: \n%.*s\n", (int) hm->body.len, hm->body.p);

            ss << hm->body.p;
            ss >> root;

            // check if is possible to free hm->body.p

            for (unsigned i = 0; i < root["hits"]["hits"].size(); i++) {
                for (int f = 0; f < vlogfields.size(); f++) {
                    try {
                        auto vfield = vlogfields[f];
                        auto name = &((*vfield).name);
                        auto value = &((*vfield).value);

                        if (*value) {
                            auto field = &(root["hits"]["hits"][i]["_source"][*name]);

                            if (*name == "tags") {
                                // manage the array, print just first tag element
                                cout << (*field)[0].asString() << " ";
                            } else {
                                cout << (*field).asString() << " ";
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

void cmdQuery(vector<std::string> input) {
    if (input.size() < 2) {
        std::cout << "Usage: " << input[0] << " <query>\n";
        return;
    }

    s_exit_flag = 0;
    std::string iquery;

    for (unsigned i = 1; i < input.size() - 1; i++)
        iquery += input[i] + " ";

    if (input.size() > 1) 
        iquery += input[input.size() - 1];

    auto restricted = (variables["restricted"] == "1");

    struct mg_mgr mgr;
    struct mg_connection *nc;

    mg_mgr_init(&mgr, NULL);

    nc = mg_connect_http(
            &mgr, 
            ev_handler, 
            constructUrl("/_all/_search").c_str(),
            header, 
            constructQuery(input[1], 
                variables["from"], 
                variables["size"], 
                variables["lte"], 
                variables["gte"],
                restricted).c_str());

    mg_set_protocol_http_websocket(nc);

    while (s_exit_flag == 0)
        mg_mgr_poll(&mgr, 1000);

    mg_mgr_free(&mgr);
}

// cmd functions

void cmdShow(vector<string> argv) {
    // update this to c++11, auto& iter = mapwhatever

    cout << "fields" << endl;
    for (std::map<std::string, logfield_t>::iterator iterator = logfields.begin(); iterator != logfields.end(); iterator++) {
        cout << " " << iterator->second.name;
        for (unsigned int i = 0; i < 14 - iterator->first.size(); i++)
            cout << " ";
        cout << ": " << iterator->second.value << endl;
    }

    cout << "variables" << endl;
    for (auto iterator = variables.begin(); iterator != variables.end(); iterator++) {
        cout << " " << iterator->first << " = " << iterator->second << endl;
    }
}

void cmdSetv(vector<string> argv) {
    if (argv.size() < 3) {
        cout << "usage: " << argv[0] << " <variable> <value>" << endl;
        return;
    }

    variables[argv[1]] =  argv[2];
}

void cmdManageField(string fieldName, bool enable) {
    std::transform(fieldName.begin(), fieldName.end(), fieldName.begin(), ::tolower);
    std::map<string, logfield_t>::iterator iter;

    if ((iter = logfields.find(fieldName)) == logfields.end()) {
        cout << fieldName << " unvalid field name" << endl;
    } else {
        logfields[fieldName].value = enable;
    }
}

void cmdEnableField(vector<string> argv) {
    cmdManageField(argv[1], true);
}

void cmdDisbleField(vector<string> argv) {
    cmdManageField(argv[1], false);
}

void cmdConnect(vector<string> argv) {
    // todo
}

void cmdTest(vector<std::string> argv) {
    std::string iquery;

    for (unsigned i = 1; i < argv.size() - 1; i++)
        iquery += argv[i] + " ";

    if (argv.size() > 1) 
        iquery += argv[argv.size() - 1];

    auto restricted = (variables["restricted"] == "1");

    cout << constructQuery(iquery, 
            variables["from"], 
            variables["size"], 
            variables["lte"], 
            variables["gte"],
            restricted).c_str() << endl;
}

// functions

void processInput(string userInput) {
    if (!userInput.size())
        return;

    if (userInput[0] == '#')
        return;

    istringstream inputStream(userInput);

    // #if GCC_VERSION > 40707
    //     vector<string> arguments { istream_iterator<string>{streamArgs}, istream_iterator<string>{} };
    // #else
        vector<string> arguments;
        string argument;
        while (inputStream >> argument) {
            arguments.push_back(argument);
        }
    // #endif

    string* commandline = static_cast<string*>(&arguments[0]);

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

int main(int argc, char *argv[], char *envp[]) {
    // register function maps
    cmdFunctions["show"] = cmdShow;
    cmdFunctions["set"] = cmdSetv;
    cmdFunctions["enable"] = cmdEnableField;
    cmdFunctions["disable"] = cmdDisbleField;
    cmdFunctions["test"] = cmdTest;
    cmdFunctions["query"] = cmdQuery;

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
