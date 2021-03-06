//
// Copyright (c) 2020, Humanitarian OpenStreetMap Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef __REPLICATION_HH__
#define __REPLICATION_HH__

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
# include "hotconfig.h"
#endif

#include <string>
#include <vector>
#include <thread>
#include <filesystem>
#include <iostream>
//#include <pqxx/pqxx>
#include <cstdlib>
#include <gumbo.h>
#include <mutex>

#include <osmium/io/any_input.hpp>
#include <osmium/builder/osm_object_builder.hpp>
#include <osmium/handler.hpp>
#include <osmium/visitor.hpp>
#include <osmium/io/any_output.hpp>

#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
namespace ssl = boost::asio::ssl;   // from <boost/asio/ssl.hpp>

#include "hotosm.hh"
#include "timer.hh"
#include "data/threads.hh"
#include "osmstats/changeset.hh"
// #include "osmstats/osmstats.hh"

namespace net = boost::asio;        // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;   // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

/// \namespace replication
namespace replication {

/// \file replication.hh
/// \brief This class downloads replication files, and adds them to the database
///
/// Replication files are used to update the existing data with changes. There
/// are 3 different intervals, minute, hourly, or daily replication files
/// are available.
/// Data on the directory structure and timestamps of replication files is
/// cached on disk, to shorten the lookup time on program startup.

// OsmChange file structure on planet
// https://planet.openstreetmap.org/replication/day
//    dir/dir/???.osc.gz
//    dir/dir/???.state.txt
// https://planet.openstreetmap.org/replication/hour
//    dir/dir/???.osc.gz
//    dir/dir/???.state.txt
// https://planet.openstreetmap.org/replication/minute/
//    dir/dir/???.osc.gz
//    dir/dir/???.state.txt
//
// Changeset file structure on planet
// https://planet.openstreetmap.org/replication/changesets/
//    dir/dir/???.osm.gz

typedef enum {minutely, hourly, daily, changeset} frequency_t;

/// \class StateFile
/// \brief Data structure for state.text files
///
/// This contains the data in a ???.state.txt file, used to identify the timestamp
/// of the changeset replication file. The replication file uses the same
/// 3 digit number as the state file.
class StateFile
{
public:
    StateFile(void) {
        //timestamp = boost::posix_time::second_clock::local_time();
        timestamp == boost::posix_time::not_a_date_time;
        sequence = 0;
    };

    /// Initialize with a state file from disk or memory
    StateFile(const std::string &file, bool memory);
    StateFile(const std::vector<unsigned char> &data)
        : StateFile (reinterpret_cast<const char *>(data.data()), true) {};

    /// Dump internal data to the terminal, used only for debugging
    void dump(void);

    /// Get the first numerical directory
    long getMajor(void) {
        std::vector<std::string> result;
        boost::split(result, path, boost::is_any_of("/"));
        return std::stol(result[4]);
    };
    long getMinor(void) {
        std::vector<std::string> result;
        boost::split(result, path, boost::is_any_of("/"));
        return std::stol(result[5]);
    };
    long getIndex(void) {
        std::vector<std::string> result;
        boost::split(result, path, boost::is_any_of("/"));
        return std::stol(result[6]);
    };

    // protected so testcases can access private data
//protected:
    std::string path;           ///< URL to this file
    ptime timestamp;            ///< The timestamp of the associated changeset file
    long sequence;              ///< The sequence number of the associated changeset file
    std::string frequency;      ///< The time interval of this change file
};

/// \class Planet
/// \brief This stores file paths and timestamps from planet.
class Planet
{
public:
    Planet(void);
    ~Planet(void);

    bool connectServer(void) {
        return connectServer(server);
    }
    bool connectServer(const std::string &server);
    bool disconnectServer(void) {
        // db->disconnect();           // close the database connection
        // db->close();                // close the database connection
        ioc.reset();                // reset the I/O conhtext
        stream.shutdown();          // shutdown the socket used by the stream
        // FIXME: this should return a real value
        return false;
    }

    std::shared_ptr<std::vector<unsigned char>> downloadFile(const std::string &file);

    /// Since the data files don't have a consistent time interval, this
    /// attempts to do a rough calculation of the probably data file,
    /// and downloads *.state.txt files till the right data file is found.
    /// Note that this can be slow as it has to download multiple files.
    std::string findData(frequency_t freq, ptime starttime);
    std::string findData(frequency_t freq, int sequence) {
        boost::posix_time::time_duration delta;
        // FIXME: this should return a real value
        std::string fixme;
        return fixme;
    };

    /// Dump internal data to the terminal, used only for debugging
    void dump(void);

    /// Scan remote directory from planet
    std::shared_ptr<std::vector<std::string>> scanDirectory(const std::string &dir);

    /// Extract the links in an HTML document. This is used
    /// to find the directories on planet for replication files
    std::shared_ptr<std::vector<std::string>> &getLinks(GumboNode* node,
                    std::shared_ptr<std::vector<std::string>> &links);

// private:
    std::string server = "planet.openstreetmap.org"; ///< planet server
    int port = 443;             ///< Network port on the server, note SSL only allowed
    int version = 11;           ///< HTTP version
    std::map<ptime, std::string> minute;
    std::map<ptime, std::string> changeset;
    std::map<ptime, std::string> hour;
    std::map<ptime, std::string> day;
    std::map<frequency_t, std::map<ptime, std::string>> states;
    std::map<frequency_t, std::string> frequency_tags;
    // These are for the boost::asio data stream
    boost::asio::io_context ioc;
    ssl::context ctx{ssl::context::sslv23_client};
    tcp::resolver resolver{ioc};
    boost::asio::ssl::stream<tcp::socket> stream{ioc, ctx};
};

// These are the columns in the pgsnapshot.replication_changes table
// id | tstamp | nodes_modified | nodes_added | nodes_deleted | ways_modified | ways_added | ways_deleted | relations_modified | relations_added | relations_deleted | changesets_applied | earliest_timestamp | latest_timestamp


/// \class Replication
/// \brief Handle replication files from the OSM planet server.
///
/// This class handfles identifying the right replication file to download,
/// and downloading it.
class Replication
{
public:
    Replication(void) {
        last_run = boost::posix_time::second_clock::local_time();
        server = "planet.openstreetmap.org";
        path = "/replication/changesets/";
        sequence = 0;
        port = 443;
        version = 11;           ///! HTTP version
    };
    // Downloading a replication requires either a sequence
    // number or a starting timestamp
    Replication(const std::string &host, ptime last, long seq) : Replication() {
        if (!host.empty()) { server = host; }
        if (!last.is_not_a_date_time()) { last_run = last; }
        if (seq > 0) { sequence = seq; }
    }
        
    Replication(std::string &host, long seq) { server = host; sequence = seq; };
    Replication(ptime last) { last_run = last; };
    Replication(long seq) { sequence = seq; };

    /// parse a replication file containing changesets
    bool readChanges(const std::string &file);

    /// Add this replication data to the changeset database
    bool mergeToDB();

    /// Download a file from planet
    std::shared_ptr<std::vector<unsigned char>> downloadFiles(const std::string &file, bool text) {
        std::vector<std::string> tmp;
        tmp.push_back(file);
        return downloadFiles(tmp, text);
    };

    std::shared_ptr<std::vector<unsigned char>> downloadFiles(std::vector<std::string> file, bool text);

    std::vector<StateFile> states; ///< Stored state.txt files
private:
    std::string server;         ///< The repliciation file server
    std::string path;           ///< Default top level path to the data files
    int port;                   ///< Network port for the server
    ptime last_run;             ///< Timestamp of the replication file
    long sequence;              ///< Sequence number of the replication
    int version;                ///< Version number of the replication
};

struct membuf: std::streambuf {
    membuf(char* base, std::ptrdiff_t n) {
        this->setg(base, base, base + n);
    }
};

}       // EOF replication

#endif  // EOF __REPLICATION_HH__
