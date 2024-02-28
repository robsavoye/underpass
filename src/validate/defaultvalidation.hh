//
// Copyright (c) 2020, 2021, 2022, 2023 Humanitarian OpenStreetMap Team
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

/// \file defaultvalidation.hh
/// \brief This file implements the default data validation

#ifndef __DEFAULTVALIDATION_HH__
#define __DEFAULTVALIDATION_HH__

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
# include "unconfig.h"
#endif

#include <boost/dll/alias.hpp>
#include "utils/yaml.hh"

// MinGW related workaround
#define BOOST_DLL_FORCE_ALIAS_INSTANTIATION

#include "validate.hh"

/// \namespace defaultvalidation
namespace defaultvalidation {

/// \class DefaultValidation
/// \brief This is the plugin class, deprived from the Validate class
class DefaultValidation : public Validate
{
public:
    DefaultValidation(void);
    ~DefaultValidation(void) {  };

    /// Check a POI for tags. A node that is part of a way shouldn't have any
    /// tags, this is to check actual POIs, like a school.
    std::shared_ptr<ValidateStatus> checkNode(const osmobjects::OsmNode &node, const std::string &type);

    /// This checks a way. A way should always have some tags. Often a polygon
    /// is a building
    std::shared_ptr<ValidateStatus> checkWay(const osmobjects::OsmWay &way, const std::string &type);


    /// This checks a relation. A relation should always have some tags.
    std::shared_ptr<ValidateStatus> checkRelation(const osmobjects::OsmRelation &relation, const std::string &type);

    // Factory method
    static std::shared_ptr<DefaultValidation> create(void) {
        return std::make_shared<DefaultValidation>();
    };
private:
    std::map<std::string, std::vector<std::string>> tests;
};

BOOST_DLL_ALIAS(DefaultValidation::create, create_plugin)

} // EOF defaultvalidation namespace

#endif  // EOF __DEFAULTVALIDATION_HH__

// Local Variables:
// mode: C++
// indent-tabs-mode: nil
// End:
