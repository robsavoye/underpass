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

#ifndef __TMUSERS_HH__
#define __TMUSERS_HH__

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
#include "unconfig.h"
#endif

#include <array>
#include <iostream>
#include <memory>
#include <pqxx/pqxx>
#include <string>
#include <vector>

#include "tmdefs.hh"

#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/date_time.hpp>
using namespace boost::posix_time;
using namespace boost::gregorian;

namespace tmdb {

/**
 * \brief The TMUser class represents a user with information extracted from the
 * TM DB.
 */
struct TMUser {

    /**
     * \brief The Gender enum maps the TM database numeric gender.
     */
    enum class Gender : int
    {
        UNSET = 0,
        MALE = 1,
        FEMALE = 2,
        SELF_DESCRIBE = 3,
        PREFER_NOT = 4
    };

    /**
     * \brief The Role enum maps the TM database numeric role.
     */
    enum class Role : int
    {
        UNSET = 0,
        READ_ONLY = 1,
        MAPPER = 2,
        ADMIN = 3
    };

    /**
     * \brief The MappingLevel enum maps the TM database numeric level.
     */
    enum class MappingLevel : int
    {
        UNSET = 0,
        BEGINNER = 1,
        INTERMEDIATE = 2,
        ADVANCED = 3
    };

    TMUser() = default;
    TMUser(pqxx::result::const_iterator &row);

    bool operator==(const TMUser &other) const;

    TaskingManagerIdType id;
    //     validation_message boolean NOT NULL,
    std::string name;
    std::string username;
    Gender gender;
    Role access;
    MappingLevel mapping_level;
    int tasks_mapped;
    int tasks_validated;
    int tasks_invalidated;
    //     email_address character varying,
    //     is_email_verified boolean,
    //     is_expert boolean,
    //     twitter_id character varying,
    //     facebook_id character varying,
    //     linkedin_id character varying,
    //     slack_id character varying,
    //     skype_id character varying,
    //     irc_id character varying,
    //     city character varying,
    //     country character varying,
    //     picture_url character varying,
    //     self_description_gender character varying,
    //     default_editor character varying NOT NULL,
    //     mentions_notifications boolean NOT NULL,
    //     comments_notifications boolean NOT NULL,
    //     projects_notifications boolean NOT NULL,
    ptime date_registered;
    ptime last_validation_date;
    std::vector<int> projects_mapped;
};

} // namespace tmdb
#endif // EOF __TMUSERS_HH__
