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

#ifndef __UNDERPASS_HH__
#define __UNDERPASS_HH__

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
# include "hotconfig.h"
#endif

#include <string>
#include <vector>
#include <iostream>
#include <pqxx/pqxx>
#include <future>

#include <thread>
#include <mutex>
#include <condition_variable>

#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/http/parser.hpp>

namespace beast = boost::beast;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace http = beast::http;
using tcp = net::ip::tcp;

#include "osmstats/replication.hh"
#include "osmstats/osmstats.hh"

namespace replication {
class StateFile;
};


/// \file underpass.hh
/// \brief Underpass for monitoring the OSM planet server for replication files.
///
/// These are the underpass used to download and apply the replication files
/// to a database. The monitor the OSM planet server for updated replication
/// files.

/// \namespace underpass
namespace underpass {

class Underpass
{
public:
    /// Connect to the Underpass database
    Underpass(void);
    Underpass(const std::string &dbname);
    ~Underpass(void);

    /// Connect ti the Underpass database
    bool connect(const std::string &database);

    void dump(void);

    /// Update the creator table to track editor statistics
    bool updateCreator(long user_id, long change_id, const std::string &editor,
                       const std::string &hashtags);

    /// Write the stored data on the directories and timestamps
    /// on the planet server.
    bool writeState(replication::StateFile &state);

    /// Get the state.txt file data by it's path
    std::shared_ptr<replication::StateFile> getState(const std::string &path);

    /// Get the state.txt date by timestamp
    std::shared_ptr<replication::StateFile> getState(replication::frequency_t freq, ptime &tstamp);

    /// Get the maximum timestamp for the state.txt data
    std::shared_ptr<replication::StateFile> getLastState(replication::frequency_t freq);

    /// Get the minimum timestamp for the state.txt data
    std::shared_ptr<replication::StateFile> getFirstState(replication::frequency_t freq);
    
// protected:
    // pqxx::connection *db;
    std::shared_ptr<pqxx::connection> sdb;
    std::map<replication::frequency_t, std::string> frequency_tags;
    //pqxx::work worker;
    std::string database = "underpass"; ///< The database to use
    std::string dbserver = "localhost"; ///< The database server to use
    int port = 443;             ///< Network port on the server, note SSL only allowed    
};

    
} // EOF underpass namespace

#endif  // EOF __UNDERPASS_HH__
