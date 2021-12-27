//
// Copyright (c) 2020, 2021 Humanitarian OpenStreetMap Team
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

#ifndef __OSMSTATS_HH__
#define __OSMSTATS_HH__

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
#include "unconfig.h"
#endif

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <pqxx/pqxx>
#include <string>
#include <vector>

#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
using namespace boost::posix_time;
using namespace boost::gregorian;

#include "validate/validate.hh"
#include "data/pq.hh"
#include "hottm/tmdefs.hh"
#include "hottm/tmusers.hh"
#include "galaxy/changeset.hh"
#include "galaxy/osmchange.hh"

using namespace tmdb;

// Forward declarations
namespace changeset {
class ChangeSet;
};
namespace osmchange {
class OsmChange;
class ChangeStats;
}; // namespace osmchange

/// \namespace galaxy
namespace galaxy {

/// \file galaxy.hh
/// \brief This file is used to work with the OSM Stats database
///
/// This manages the OSM Stats schema in a postgres database. This
/// includes querying existing data in the database, as well as
/// updating the database.

/// \class RawChangeset
/// \brief This is the data structure for a raw changeset
///
/// The raw_changesets table contains all the calculated statistics
/// for a change. This stores the data as parsed from the database.
class RawChangeset {
  public:
    RawChangeset(pqxx::const_result_iterator &res);
    RawChangeset(const std::string filespec);
    void dump(void);
    // protected:
    // These are from the OSM Stats 'raw_changesets' table
    std::map<std::string, long> counters;
    long id;                           ///< The change ID
    std::string editor;                ///< The editor used for this change
    long user_id;                      ///< The user ID
    ptime created_at;                  ///< The starting timestamp
    ptime closed_at;                   ///< The finished timestamp
    bool verified;                     ///< Whether this change has been validated
    std::vector<long> augmented_diffs; ///< The diffs, currently unused
    ptime updated_at;                  ///< Time this change was updated
    //
    long updateCounter(const std::string &key, long value)
    {
        counters[key] = value;
        // FIXME: this should return a real value
        return 0;
    };
    long operator[](const std::string &key) { return counters[key]; };
};

/// \class RawUser
/// \brief Stores the data from the raw user table
///
/// The raw_user table is used to coorelate a user ID with their name.
/// This stores the data as parsed from the database.
class RawUser {
  public:
    RawUser(void){};
    /// Instantiate the user data from an iterator
    RawUser(pqxx::const_result_iterator &res)
    {
        id = res[0].as(int(0));
        name = res[1].c_str();
    }
    /// Instantiate the user data
    RawUser(long uid, const std::string &tag)
    {
        id = uid;
        name = tag;
    }
    int id;           ///< The users OSM ID
    std::string name; ///< The users OSM username
};

/// \class RawHashtag
/// \brief Stores the data from the raw hashtag table
///
/// The raw_hashtag table is used to coorelate a hashtag ID with the
/// hashtag name. This stores the data as parsed from the database.
class RawHashtag {
  public:
    RawHashtag(void){};
    /// Instantiate the hashtag data from an iterator
    RawHashtag(pqxx::const_result_iterator &res)
    {
        id = res[0].as(int(0));
        name = res[1].c_str();
    }
    /// Instantiate the hashtag data
    RawHashtag(int hid, const std::string &tag)
    {
        id = hid;
        name = tag;
    }
    int id = 0;       ///< The hashtag ID
    std::string name; ///< The hashtag value
};

/// \class QueryGalaxy
/// \brief This handles all direct database access
///
/// This class handles all the queries to the OSM Stats database.
/// This includes querying the database for existing data, as
/// well as updating the data whenh applying a replication file.
class QueryGalaxy : public pq::Pq {
  public:
    QueryGalaxy(void);
    QueryGalaxy(const std::string &dburl);
    /// close the database connection
    ~QueryGalaxy(void){};

    bool readGeoBoundaries(const std::string &rawfile) { return false; };

    /// Populate internal storage of a few heavily used data, namely
    /// the indexes for each user, country, or hashtag.
    bool populate(void);

    /// Read changeset data from the galaxy database
    bool getRawChangeSets(std::vector<long> &changeset_id);

    /// Add a user to the internal data store
    int addUser(long id, const std::string &user)
    {
        RawUser ru(id, user);
        users.push_back(ru);
        return users.size();
    };
    /// Add a hashtag to the internal data store
    int addHashtag(int id, const std::string &tag)
    {
        RawHashtag rh(id, tag);
        hashtags[tag] = rh;
        return hashtags.size();
    };

    /// Add a comment and their ID to the database
    int addComment(long id, const std::string &user);

    /// Apply a change to the database
    bool applyChange(const changeset::ChangeSet &change) const;
    bool applyChange(const osmchange::ChangeStats &change) const;
    bool applyChange(const ValidateStatus &validation) const;
    bool updateValidation(long id);

    int lookupHashtag(const std::string &hashtag);
    bool hasHashtag(long changeid);
    // Get the timestamp of the last update in the database
    ptime getLastUpdate(void);
    // private:

    long queryData(long cid, const std::string &column)
    {
        std::string query = "SELECT " + column + " FROM raw_changesets";
        query += " WHERE id=" + std::to_string(cid);
        std::cout << "QUERY: " << query << std::endl;
        pqxx::work worker(*sdb);
        pqxx::result result = worker.exec(query);
        worker.commit();

        // FIXME: this should return a real value
        return 0;
    }

    /**
     * \brief The SyncResult struct represents the result of a
     *        synchronization operation.
     */
    struct SyncResult {
        unsigned long created = 0;
        unsigned long updated = 0;
        unsigned long deleted = 0;

        bool operator==(const SyncResult &other) const
        {
            return created == other.created && updated == other.updated &&
                   deleted == other.deleted;
        }

        /**
         * \brief clear the sync result by resetting all counters to 0.
         */
        void clear()
        {
            created = 0;
            updated = 0;
            deleted = 0;
        }
    };

    /**
     * \brief syncUsers synchronize users from TM DB into Underpass DB.
     * \param users list of users from TM DB to be synced.
     * \param purge default to FALSE, if TRUE users that are not in \a users but
     *        still present in the Galaxy DB will be deleted.
     * \return a SyncResult object.
     */
    SyncResult syncUsers(const std::vector<TMUser> &users, bool purge = false);

    std::string db_url;
    std::vector<RawUser> users; ///< All the raw user data
    std::map<std::string, RawHashtag> hashtags;

  private:
    mutable std::mutex changes_write_mutex;
};

} // namespace galaxy

#endif // EOF __GALAXY_HH__
