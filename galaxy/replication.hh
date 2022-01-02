//
// Copyright (c) 2020, 2021, 2022 Humanitarian OpenStreetMap Team
//
// This file is part of Underpass.
//
//     Underpass is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     Underpass is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with Underpass.  If not, see <https://www.gnu.org/licenses/>.
//

#ifndef __REPLICATION_HH__
#define __REPLICATION_HH__

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
#include "unconfig.h"
#endif

#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
//#include <pqxx/pqxx>
#include <cstdlib>
#include <gumbo.h>
#include <mutex>

#include <osmium/builder/osm_object_builder.hpp>
#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/visitor.hpp>

#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/date_time.hpp>
using namespace boost::posix_time;
using namespace boost::gregorian;
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
#include <boost/format.hpp>
using boost::format;

#include "galaxy/changeset.hh"

namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp;         // from <boost/asio/ip/tcp.hpp>

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

typedef enum { minutely, hourly, daily, changeset } frequency_t;

/// \class StateFile
/// \brief Data structure for state.txt files
///
/// This contains the data in a ???.state.txt file, used to identify the timestamp
/// of the changeset replication file. The replication file uses the same
/// 3 digit number as the state file.
class StateFile {
  public:
    ///
    /// \brief constructs an invalid StateFile
    /// \see isValid()
    StateFile(void)
    {
        timestamp = not_a_date_time;
        sequence = -1; // Turns out 0 is a valid sequence for changeset :/
    };

    /// Initialize with a state file from disk or memory
    StateFile(const std::string &file, bool memory);
    StateFile(const std::vector<unsigned char> &data)
        : StateFile(reinterpret_cast<const char *>(data.data()), true){};

    inline bool operator==(const StateFile &other) const
    {
        return timestamp == other.timestamp && sequence == other.sequence &&
               path == other.path && frequency == other.frequency;
    };

    inline bool operator!=(const StateFile &other) { return !(*this == other); }

    /// Dump internal data to the terminal, used only for debugging
    void dump(void);

    /// Get the first numerical directory
    long getMajor(void)
    {
        std::vector<std::string> result;
        boost::split(result, path, boost::is_any_of("/"));
        return std::stol(result[1]);
    };
    long getMinor(void)
    {
        std::vector<std::string> result;
        boost::split(result, path, boost::is_any_of("/"));
        return std::stol(result[2]);
    };
    long getIndex(void)
    {
        std::vector<std::string> result;
        boost::split(result, path, boost::is_any_of("/"));
        return std::stol(result[3]);
    };

    ///
    /// \brief isValid checks the validity of a state file.
    /// A state file is considered valid is it has a not-null timestamp, a path, a frequence and a valid sequence.
    /// \return TRUE if the StateFile is valid.
    ///
    bool isValid() const;

    ///
    /// \brief freq_to_string returns a string representation of the given \a frequency.
    /// The string representation can be used as part of the path for replication URL
    /// (day, hour, minute, changesets).
    /// \param frequency the frequency enum value.
    /// \return a string representation of the frequency.
    ///
    static std::string freq_to_string(frequency_t frequency) {
	std::map<replication::frequency_t, std::string> frequency_tags = {
	    {replication::minutely, "minute"},
	    {replication::hourly, "hour"},
	    {replication::daily, "day"},
	    {replication::changeset, "changesets"}};
        return frequency_tags[frequency];
    };
    ///
    /// \brief freq_from_string returns a frequency from its string representation \a frequency_str.
    /// \param frequency_str the string representation (day, hour, minute, changesets).
    /// \return the enum value for \a frequency_str.
    /// \throws std::invalid_argument if the \a frequency_str is not a valid frequency.
    ///
    static frequency_t freq_from_string(const std::string &frequency_str) {
	std::map<const std::string, replication::frequency_t> frequency_values = {
	    {"minute", replication::minutely},
	    {"hour", replication::hourly},
	    {"day", replication::daily},
	    {"changesets", replication::changeset}};
        return frequency_values[frequency_str];
    };

    // FIXME: protected so testcases can access private data, this should be fixed
    //protected:
    std::string path; ///< URL to this file
    ptime timestamp = not_a_date_time; ///< The timestamp of the associated changeset file
    long sequence = -1; ///< The sequence number of the associated changeset/osmchange file
    // FIXME: frequency is stored as a string, an ENUM would be a better choice, DB schema should be changed accordingly,.
    frequency_t frequency; ///< The time interval of this change file
};

/// \class RemoteURL
/// \brief This parses a remote URL into pieces
class RemoteURL {
  public:
    RemoteURL(void);
    RemoteURL(const RemoteURL &inr);
    RemoteURL(const std::string &rurl) { parse(rurl); }
    /// Parse a URL into it's elements
    void parse(const std::string &rurl);
    void updatePath(int major, int minor, int index);
    std::string domain;
    std::string datadir;
    std::string subpath;
    frequency_t frequency;
    std::string base;
    std::string url;
    int major;
    int minor;
    int index;
    std::string filespec;
    std::string destdir;
    void replacePlanet(const std::string &new_domain, const std::string &new_datadir);
    void dump(void);
    void Increment(void);
    RemoteURL &operator=(const RemoteURL &inr);
    long sequence() const;
};

/// \class Planet
/// \brief This stores file paths and timestamps from planet.
class Planet {
  public:
    Planet(void);
    // Planet(const std::string &planet) { pserver = planet; };
    Planet(const RemoteURL &url)
    {
        remote = url;
        if (!connectServer(url.domain)) {
            throw std::runtime_error("Error connecting to server " + url.domain);
        }
    };
    ~Planet(void);

    bool connectServer(void) { return connectServer(remote.domain); }
    bool connectServer(const std::string &server);
    bool disconnectServer(void)
    {
        // db->disconnect();           // close the database connection
        // db->close();                // close the database connection
        ioc.reset();       // reset the I/O conhtext
        stream.shutdown(); // shutdown the socket used by the stream
        // FIXME: this should return a real value
        return false;
    }

    ///
    /// \brief downloadFile downloads a file from planet
    /// \param file the full URL or the path part of the URL (such as: "/replication/changesets/000/001/633.osm.gz"), the host part is taken from remote.domain.
    /// \return file data (possibly empty in case of errors)
    ///
    std::shared_ptr<std::vector<unsigned char>>
    downloadFile(const std::string &file);

    ///
    /// \brief fetchData
    /// \param freq
    /// \param path
    /// \param underpass_dburl
    /// \return
    ///
    std::shared_ptr<StateFile>
    fetchData(frequency_t freq, const std::string &path, const std::string &underpass_dburl);

    static std::string sequenceToPath(long sequence);

    ///
    /// \brief fetchLastData reads the replication/<frequency>/state.txt file and retieve the last ???.state.txt state.
    ///        Data is never read from cache but if \a underpass_dburl is not empty, retrieved state will be saved in the DB.
    /// \param freq frequency.
    /// \param underpass_dburl optional url for underpass DB where data are cached, an empty value means no cache will be used.
    /// \return the last (possily invalid) state.
    ///
    std::shared_ptr<StateFile> fetchDataLast(frequency_t freq, const std::string &underpass_dburl);

    ///
    /// \brief fetchLastFirst reads the replication/<frequency>/state.txt file and retieve the first ???.state.txt state.
    ///        Sequence == 1 is attempted first and read from cache (if \a underpass_dburl is not empty and \a force_scan is FALSE),
    ///        if that fails an index scan is performed in order to identify the first valid state, retrieved state will be
    ///        saved in the DB.
    /// \param freq frequency.
    /// \param underpass_dburl optional url for underpass DB where data are cached, an empty value means no cache will be used.
    /// \param force_scan optional flag to force an index scan (mainly for testing purposes).
    /// \return the first (possily invalid) state.
    ///
    std::shared_ptr<StateFile> fetchDataFirst(frequency_t freq, const std::string &underpass_dburl, bool force_scan = false);

    ///
    /// \brief fetchData retrieves data from the cache or from the server.
    /// \param freq frequency.
    /// \param timestamp timestamp.
    /// \param underpass_dburl optional url for underpass DB where data are cached, an empty value means no cache will be used.
    /// \return a (possibly invalid) StateFile record.
    ///
    std::shared_ptr<StateFile>
    fetchData(frequency_t freq, ptime timestamp, const std::string &underpass_dburl);

    std::shared_ptr<StateFile>
    fetchData(frequency_t freq, long sequence, const std::string &underpass_dburl);

    ///
    /// \brief fetchDataGreaterThan finds and returns the first (possibly invalid) state with a sequence greater than \a sequence.
    /// \param freq frequency.
    /// \param sequence sequence.
    /// \param underpass_dburl optional url for underpass DB where data are cached, an empty value means no cache will be used.
    /// \return the first (possibly invalid) state with a sequence greater than \a sequence.
    ///
    std::shared_ptr<StateFile>
    fetchDataGreaterThan(frequency_t freq, long sequence, const std::string &underpass_dburl);

    ///
    /// \brief fetchDataGreaterThan finds and returns a (possibly invalid) state with a timestamp greater than \a sequence.
    /// If the optional \a underpass_dburl is specified and the DB already contains a state with a timestamp greater than
    /// the requested \a timestamp then that state is returned. In any other case the last available state is fetched from the
    /// server and returned if the timestamp is greater than the requested \a timestamp.
    /// \param freq frequency.
    /// \param timestamp timestamp.
    /// \param underpass_dburl optional url for underpass DB where data are cached, an empty value means no cache will be used.
    /// \return the first (possibly invalid) state with a sequence greater than \a sequence.
    ///
    std::shared_ptr<StateFile>
    fetchDataGreaterThan(frequency_t freq, ptime timestamp, const std::string &underpass_dburl);

    ///
    /// \brief fetchDataLessThan finds and returns a (possibly invalid) state with a sequence less than \a sequence.
    /// \param freq frequency.
    /// \param sequence sequence.
    /// \param underpass_dburl optional url for underpass DB where data are cached, an empty value means no cache will be used.
    /// \return the first (possibly invalid) state with a sequence less than \a sequence.
    ///
    std::shared_ptr<StateFile>
    fetchDataLessThan(frequency_t freq, long sequence, const std::string &underpass_dburl);

    std::shared_ptr<StateFile>
    fetchDataLessThan(frequency_t freq, ptime timestamp, const std::string &underpass_dburl);

    /// Dump internal data to the terminal, used only for debugging
    void dump(void);

    /// Scan remote directory from planet
    std::shared_ptr<std::vector<std::string>> scanDirectory(const std::string &dir);

    /// Extract the links in an HTML document. This is used
    /// to find the directories on planet for replication files
    std::shared_ptr<std::vector<std::string>> &getLinks(GumboNode *node, std::shared_ptr<std::vector<std::string>> &links);

    // private:
    RemoteURL remote;
    //    std::string pserver;        ///< The replication file server
    //    std::string datadir;        ///< Default top level path to the data files
    //    replication::frequency_t frequency;
    int port = 443;   ///< Network port on the server, note SSL only allowed
    int version = 11; ///< HTTP version
    std::map<ptime, std::string> minute;
    std::map<ptime, std::string> changeset;
    std::map<ptime, std::string> hour;
    std::map<ptime, std::string> day;
    std::map<frequency_t, std::map<ptime, std::string>> states;
    // These are for the boost::asio data stream
    boost::asio::io_context ioc;
    ssl::context ctx{ssl::context::sslv23_client};
    tcp::resolver resolver{ioc};
    boost::asio::ssl::stream<tcp::socket> stream{ioc, ctx};
    //    std::string baseurl;        ///< URL for the planet server
};

// These are the columns in the pgsnapshot.replication_changes table
// id | tstamp | nodes_modified | nodes_added | nodes_deleted | ways_modified | ways_added | ways_deleted | relations_modified | relations_added | relations_deleted | changesets_applied | earliest_timestamp | latest_timestamp

/// \class Replication
/// \brief Handle replication files from the OSM planet server.
///
/// This class handles identifying the right replication file to download,
/// and downloading it.
class Replication {
  public:
    Replication(void)
    {
        last_run = boost::posix_time::second_clock::local_time();
        sequence = 0;
        port = 443;
        version = 11; ///! HTTP version
    };
    // Downloading a replication requires either a sequence
    // number or a starting timestamp
    Replication(ptime last, long seq) : Replication()
    {
        if (!last.is_not_a_date_time()) {
            last_run = last;
        }
        if (seq > 0) {
            sequence = seq;
        }
    }

    Replication(ptime last) { last_run = last; };
    Replication(long seq) { sequence = seq; };

    /// parse a replication file containing changesets
    bool readChanges(const std::string &file);

    /// Add this replication data to the changeset database
    bool mergeToDB();

    std::vector<StateFile> states; ///< Stored state.txt files
  private:
    std::string pserver; ///< The replication file server
    int port;            ///< Network port for the server
    ptime last_run;      ///< Timestamp of the replication file
    long sequence;       ///< Sequence number of the replication
    int version;         ///< Version number of the replication
};

struct membuf : std::streambuf {
    membuf(char *base, std::ptrdiff_t n) { this->setg(base, base, base + n); }
};

} // namespace replication

#endif // EOF __REPLICATION_HH__

// local Variables:
// mode: C++
// indent-tabs-mode: t
// End:
