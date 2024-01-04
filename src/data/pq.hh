//
// Copyright (c) 2020, 2021, 2022, 2023 Humanitarian OpenStreetMap Team
//
// This file is part of Underpass.
//
//     Pq is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     Pq is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with Pq.  If not, see <https://www.gnu.org/licenses/>.
//

#ifndef __PQ_HH__
#define __PQ_HH__

/// \file pq.hh
/// \brief Pq for monitoring the OSM planet server for replication files.
///
/// These are the pq used to download and apply the replication files
/// to a database. The monitor the OSM planet server for updated replication
/// files.

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
#include "unconfig.h"
#endif

#include <iostream>
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <mutex>

/// \namespace pq
namespace pq {

/// \class Pq
/// \brief This is a higher level class wrapped around libpqxx
class Pq {
  public:
    Pq();

    /// Connect to the Pq database
    Pq(const std::string &dbname);
    bool connect(const std::string &args);

    /// \brief isOpen, checks if the DB is open.
    /// \return TRUE if the DB is open.
    bool isOpen() const;

    /// Run query into the database
    pqxx::result query(const std::string &query);
    /// Parse the URL for the database connection
    bool parseURL(const std::string &query);

    /// Dump internal data for debugging
    void dump(void);

    // Escape string
    std::string escapedString(const std::string &s);

    // Escape JSON
    std::string escapedJSON(const std::string &s);

    // Database connection
    std::shared_ptr<pqxx::connection> sdb;

    //protected:
    pqxx::result result;  ///< The result from a query
    std::string host;  ///< The database host
    std::string port;  ///< The database port
    std::string user;  ///< The database user
    std::string passwd;  ///< The database password
    std::string dbname;  ///< The database name
    std::mutex pqxx_mutex;

};

} // namespace pq

#endif // EOF __PQ_HH__

// local Variables:
// mode: C++
// indent-tabs-mode: nil
// End:
