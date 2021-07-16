//
// Copyright (c) 2020, 2021 Humanitarian OpenStreetMap Team
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

#ifndef __OSMCHANGE_HH__
#define __OSMCHANGE_HH__

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
# include "unconfig.h"
#endif

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <iostream>
//#include <pqxx/pqxx>
#ifdef LIBXML
# include <libxml++/libxml++.h>
#endif
#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;
#define BOOST_BIND_GLOBAL_PLACEHOLDERS 1
#include <boost/progress.hpp>

#include "hotosm.hh"
#include "data/osmobjects.hh"
#include "osmstats/osmchange.hh"

#include <ogr_geometry.h>

// forward declare geometry typedefs
// namespace geoutil {
// typedef boost::geometry::model::d2::point_xy<double> point_t;
// typedef boost::geometry::model::linestring<point_t> linestring_t;
// typedef boost::geometry::model::polygon<point_t> polygon_t;
// typedef boost::geometry::model::multi_polygon<polygon_t> multipolygon_t ;
// };

/// \namespace osmchange
namespace osmchange {

/// \file osmchange.hh
/// \brief This file parses a change file in the OsmChange format
///
/// This file parses an OsmChange formatted data file using an libxml++
/// SAX parser, which works better for large files, or boost parse trees
/// using a DOM parser, which is faster for small files.

/// \enum action_t
/// The actions supported by OsmChange files
//typedef enum {none, create, modify, remove} action_t; // delete is a reserved word
/// \enum osmtype_t
/// The object types used by an OsmChange file
typedef enum {empty, node, way, relation, member} osmtype_t;

/// \class ChangeStats
/// \brief These are per user statistics
///
/// This stores the calculated data from a change for a user,
/// which later gets added to the database statistics.
class ChangeStats
{
public:
    long change_id = 0;             ///< The ID of this change
    long user_id = 0;               ///< The User ID
    std::string username;	    ///< The User Name
    ptime created_at;               ///< The starting timestamp
    ptime closed_at;                ///< The finished timestamp
    std::map<std::string, int> added;
    std::map<std::string, int> modified;
    std::map<std::string, int> deleted;
    /// Dump internal data to the terminal, only for debugging
    void dump(void);
};

/// \class OsmChange
/// \brief This contains the data for a change
///
/// This contains all the data in a change. Redirection is used
/// so the object type has a generic API
class OsmChange
{
public:
    OsmChange(osmobjects::action_t act) { action = act; };
    
    ///< dump internal data, for debugging only
    void dump(void);
    
// protected:
    /// Set the latitude of the current node
    void setLatitude(double lat) {
        if (type == node) { nodes.back()->setLatitude(lat); }
    };
    /// Set the longitude of the current node
    void setLongitude(double lon) {
        if (type == node) { nodes.back()->setLongitude(lon); }
    };
    /// Set the timestamp of the current node or way
    void setTimestamp(const std::string &val) {
        if (type == node) { nodes.back()->timestamp = time_from_string(val); }
        if (type == way) { ways.back()->timestamp = time_from_string(val); }
    };
    /// Set the version number of the current node or way
    void setVersion(double val) {
        if (type == node) { nodes.back()->version = val; }
        if (type == way) { ways.back()->version = val; }
    };
    /// Add a tag to the current node or way
    void addTag(const std::string &key, const std::string &value) {
        if (type == node) { nodes.back()->addTag(key, value); }
        if (type == way) { ways.back()->addTag(key, value); }
    };
    /// Add a node reference to the current way
    void addRef(long ref) {
        if (type == way) { ways.back()->addRef(ref); }
    };
    /// Set the User ID for the current node or way
    void setUID(long val) {
        if (type == node) { nodes.back()->uid = val; }
        if (type == way) { ways.back()->uid = val; }
    };
    /// Set the Change ID for the current node or way
    void setChangeID(long val) {
        if (type == node) { nodes.back()->id = val; }
        if (type == way) { ways.back()->id = val; }
    };
    /// Set the User name for the current node or way
    void setUser(const std::string &val) {
        if (type == node) { nodes.back()->user = val; }
        if (type == way) { ways.back()->user = val; }
    };
    /// Instantiate a new node
    std::shared_ptr<osmobjects::OsmNode> newNode(void) {
        auto tmp = std::make_shared<osmobjects::OsmNode>();
        type = node;
        nodes.push_back(tmp);
        return tmp;
    };
    /// Instantiate a new way
    std::shared_ptr<osmobjects::OsmWay> newWay(void) {
        std::shared_ptr<osmobjects::OsmWay> tmp = std::make_shared<osmobjects::OsmWay>();
        type = way;
        ways.push_back(tmp);
        return tmp;
    };
    /// Instantiate a new relation    
    std::shared_ptr<osmobjects::OsmRelation> newRelation(void) {
        std::shared_ptr<osmobjects::OsmRelation> tmp = std::make_shared<osmobjects::OsmRelation>();
        type = relation;
        relations.push_back(tmp);
        return tmp;
    };

    /// Get a specific node in this change
    std::shared_ptr<osmobjects::OsmNode> getNode(int index) { return nodes[index]; };
    /// Get the current node in this change
    std::shared_ptr<osmobjects::OsmNode> currentNode(void) { return nodes.back(); };
    /// Get a specific way in this change
    std::shared_ptr<osmobjects::OsmWay> getWay(int index) { return ways[index]; };
    /// Get a specific relation in this change
    std::shared_ptr<osmobjects::OsmRelation> getRelation(int index) { return relations[index]; };

    osmobjects::action_t action = osmobjects::none;      ///< The change action
    osmtype_t type;                                      ///< The OSM object type
    std::vector<std::shared_ptr<osmobjects::OsmNode>> nodes;         ///< The nodes in this change
    std::vector<std::shared_ptr<osmobjects::OsmWay>> ways;           ///< The ways in this change
    std::vector<std::shared_ptr<osmobjects::OsmRelation>> relations; ///< The relations in this change
    std::shared_ptr<osmobjects::OsmObject> obj;
};

/// \class OsmChangeFile
/// \brief This class manages an OSM change file.
///
/// This class handles the entire OsmChange file. Currently libxml++
/// is used with a SAX parser, although optionally the boost parse tree
/// can be used as a DOM parser.
#ifdef LIBXML
class OsmChangeFile : public xmlpp::SaxParser
#else
class OsmChangeFile
#endif
{
public:
    OsmChangeFile(void) { };
    OsmChangeFile(const std::string &osc) { readChanges(osc); };

    /// Read a changeset file from disk or memory into internal storage
    bool readChanges(const std::string &osc);
    
#ifdef LIBXML
    /// Called by libxml++ for each element of the XML file
    void on_start_element(const Glib::ustring& name,
                          const AttributeList& properties) override;

#endif
    /// Read an istream of the data and parse the XML
    bool readXML(std::istream & xml);

    std::map<long, std::shared_ptr<ChangeStats>> userstats; ///< User statistics for this file
    
    std::vector<std::shared_ptr<OsmChange>> changes; ///< All the changes in this file

    /// Collect statistics for each user
    std::shared_ptr<std::map<long, std::shared_ptr<ChangeStats>>> collectStats(const multipolygon_t &poly);

    std::shared_ptr<std::vector<std::string>> scanTags(std::map<std::string, std::string> tags);

    /// dump internal data, for debugging only
    void dump(void);
};
    
}       // EOF osmchange

#endif  // EOF __OSMCHANGE_HH__

// local Variables:
// mode: C++
// indent-tabs-mode: t
// End:
