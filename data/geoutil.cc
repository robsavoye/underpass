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

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
# include "hotconfig.h"
#endif

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <utility>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <deque>
#include <list>

#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/algorithm/string.hpp>

#include "hotosm.hh"
#include "osmstats/osmstats.hh"
#include "osmstats/changeset.hh"
#include "data/geoutil.hh"

#include <gdal/ogrsf_frmts.h>

namespace geoutil {


int
GeoCountry::extractTags(const std::string &other)
{
    std::vector<std::string> tags;
    boost::split(tags, other, boost::is_any_of(","));
    for (int i = 0; i < tags.size(); i++) {
        std::size_t pos = tags[i].find('=');
        if (pos != std::string::npos) {
            std::string key = tags[i].substr(0, pos);
            key.erase(std::remove(key.begin(),key.end(),'\"'),key.end());
            std::string value = tags[i].substr(pos + 2, std::string::npos);
            // Remove the double quotes
            key.erase(std::remove(key.begin(),key.end(),'\"'),key.end());
            value.erase(std::remove(value.begin(),value.end(),'\"'),value.end());
            names[key] = value;
            if (key == "name:iso_w2" || key == "name:iso_w3") {
                if (value.size() == 2) {
                    iso_a2 = value;
                } else {
                    iso_a3 = value;
                }
            }
            if (key == "cid") {
                id = std::stol(value);
            }
        }
    }

    return tags.size();
}

// bool
// GeoUtil::connect(const std::string &dbserver, const std::string &dbname)
// {
//     std::string args;
//     if (dbname.empty()) {
// 	args = "dbname = geobundaries";
//     } else {
// 	args = "dbname = " + dbname;
//     }
    
//     try {
// 	db = new pqxx::connection(args);
// 	if (db->is_open()) {
//             // worker = new pqxx::work(*db);
// 	    return true;
// 	} else {
// 	    return false;
// 	}
//     } catch (const std::exception &e) {
// 	std::cerr << e.what() << std::endl;
// 	return false;
//    }    
// }

bool
GeoUtil::readFile(const std::string &filespec, bool multi)
{
    GDALDataset       *poDS;
    std::string infile = filespec;
    if (filespec.empty()) {
        infile = "/include.osm";
    }

    std::cout << "Opening geo data file: " << infile << std::endl;
    poDS = (GDALDataset*) GDALOpenEx(infile.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
    if (poDS == 0) {
        std::cout << "ERROR:couldn't open " << infile << std::endl;
        return false;
    }

    OGRLayer *layer;
    layer = poDS->GetLayerByName( "multipolygons" );
    if (layer == 0) {
        std::cout << "ERROR: Couldn't get layer \"multipolygons\"" << std::endl;
        return false;
    }

    if (layer != 0) {
        for (auto& feature: layer) {
            GeoCountry country;
            const OGRGeometry* geom = feature->GetGeometryRef();
            for (auto&& field: *feature) {
                if(NULL != geom) {
                    int eType = wkbFlatten(layer->GetGeomType());
                    std::string value = field.GetAsString();
                    if (strcmp(field.GetName(), "name") == 0) {
                        country.setName(value);
                    }
                    if (strcmp(field.GetName(), "alt_name") == 0) {
                        country.setAltName(value);
                    }
                    if (strcmp(field.GetName(), "other_tags") == 0) {
                        country.extractTags(value);
                    }
                }
            }
            char* wkt1 = NULL;
            const OGRMultiPolygon *mp = geom->toMultiPolygon();
            if (!multi) {
                mp->exportToWkt(&wkt1);
                // FIXME: add filtering by polygon!
                // boost::geometry::read_wkt(wkt1, boundary);
            } else {
                mp->exportToWkt(&wkt1);
                for (auto it = mp->begin(); it != mp->end(); ++it) {
                    const OGRPolygon *poly = *it;
                    poly->exportToWkt(&wkt1);
                    country.addBoundary(wkt1);
                }
                countries.push_back(country);
            }
            CPLFree(wkt1);
        }
    }

    // FIXME: return something real
    return false;
}

bool
GeoUtil::focusArea(double lat, double lon)
{

    return false;
}

// osmstats::RawCountry
// GeoUtil::findCountry(double lat, double lon)
// {

// }

void
GeoUtil::dump(void)
{
    // std::cout << "Boundaries: " << boost::geometry::wkt(boundaries) << std::endl;
    for (auto it = std::begin(countries); it != std::end(countries); ++it) {
        it->dump();
    }
}

GeoCountry &
GeoUtil::inCountry(double max_lat, double max_lon, double min_lat, double min_lon)
{
    for (auto it = std::begin(countries); it != std::end(countries); ++it) {
        bool in = it->inCountry(max_lat, max_lon, min_lat, min_lon);
        if (in) {
            return *it;
        }
    }
}

std::vector<osmstats::RawCountry>
GeoUtil::exportCountries(void)
{
    std::vector<osmstats::RawCountry> countries;
    for (auto it = std::begin(countries); it != std::end(countries); ++it) {
        osmstats::RawCountry country(it->id, it->name, "iso");
        countries.push_back(country);
    }
    return countries;
}

void
GeoCountry::dump(void)
{
    std::cout << "Country Name: " << name << std::endl;
    if (!alt_name.empty()) {
        std::cout << "Alternate Name: " << name << std::endl;
    }
    if (!iso_a2.empty()) {
        std::cout << "ISO A2: " << iso_a2 << std::endl;
    }
    if (!iso_a3.empty()) {
        std::cout << "ISO A3: " << iso_a3 << std::endl;
    }
    std::cout << "Country ID: " << id << std::endl;

    std::cout << "Boundary: " << boost::geometry::wkt(boundary) << std::endl;
    for (auto it = std::begin(names); it != std::end(names); ++it) {
        //std::cout << *it.first() << std::endl;
    }
}

bool
GeoCountry::inCountry(double max_lat, double max_lon, double min_lat, double min_lon)
{
    // Build the country outer boundary polygon
    polygon_t location;
    boost::geometry::append(location.outer(), point_t(min_lon, min_lat));
    boost::geometry::append(location.outer(), point_t(min_lon, max_lat));
    boost::geometry::append(location.outer(), point_t(max_lon, max_lat));
    boost::geometry::append(location.outer(), point_t(max_lon, min_lat));
    boost::geometry::append(location.outer(), point_t(min_lon, min_lat));

    // FIXME: It might be faster to just test for the 4 points than
    // using a polygon
    // bool ret = boost::geometry::within(point_t(min_lon, min_lat), boundary);
    bool ret = boost::geometry::within(location, boundary);

    return ret;
}


}       // EOF geoutil

