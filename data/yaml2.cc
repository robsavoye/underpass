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

/// \file yaml2.cc
/// \brief Simple YAML file reader.
///
/// Read in a YAML config file and create a nested data structure so it can be accessed.

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
# include "unconfig.h"
#endif

#include "yaml2.hh"

namespace yaml2 {

void Yaml2::read(const std::string &fspec) {
    std::ifstream yaml;
    std::string line;
    filespec = fspec;
    yaml.open(filespec,  std::ifstream::in);
    while (getline(yaml, line)) {
        if (line.size() > 0 && line[0] != '#') {
            auto lineinfo = this->process_line(line);
            auto line = lineinfo.first;
            auto index = lineinfo.second;
            Node node;
            this->clean(line);
            node.value = line;
            add_node(node, index, root);
        }
    }
}

void Yaml2::add_node(Node &node, int index, Node &parent, int depth) {
    if (index == 0) {
        parent.children.push_back(node);
    } else {
        if (depth == index) {
            parent.children.push_back(node);
        } else {
            add_node(node, index, parent.children[parent.children.size() - 1], ++depth);
        }
    }
}

void Yaml2::clean(std::string &line) {
    if (line.front() == '-') {
        line.erase(0,2);
    }
    if (line.back() == ':') {
        line.pop_back();
    }
}

std::pair<std::string, int> Yaml2::process_line(std::string line ) {
    int pos = 0;
    this->ident_count = 0;
    line = scan_ident(line);
    if (this->ident_count != 0 && this->ident_len == 0) {
        this->ident_len = this->ident_count;
    }
    if (this->ident_count != 0) {
        pos = this->ident_count / this->ident_len;
    }
    ++this->linenumber;
    return std::pair(line, pos);
}

std::string Yaml2::scan_ident(std::string line) {
    const char *c = line.substr(0, 1).c_str();
    if (ident_char == '\0') {
        if (*c == idents[0]) {
            this->ident_char = idents[0];
        } else if (*c == idents[1]) {
            this->ident_char = idents[1];
        }
    }
    if (*c == ident_char) {
        ++this->ident_count;
        return this->scan_ident(line.substr(1, line.size()));
    } else {
        return line;
    }

}

bool Yaml2::contains_key(std::string key) {
    bool result = false;
    this->contains_key(key, this->root, result);
    return result;
}

void Yaml2::contains_key(std::string key, Node &node, bool &result) {
    for (auto it = std::begin(node.children); it != std::end(node.children); ++it) {
        if (it->value == key) {
            result = true;
            return;
        }
        if (it->children.size() > 0) {
            this->contains_key(key, *it, result);
        }
    }
}

bool Yaml2::contains_value(std::string key, std::string value) {
    bool result = false;
    this->contains_value(key, value, this->root, result);
    return result;
}

void Yaml2::contains_value(std::string key, std::string value, Node &node, bool &result) {
    for (auto it = std::begin(node.children); it != std::end(node.children); ++it) {
        if (it->value == key) {
            return this->contains_key(value, *it, result);
        }
        if (it->children.size() > 0) {
            this->contains_value(key, value, *it, result);
        }
    }
}


} // EOF yaml2 namespace

// Local Variables:
// mode: C++
// indent-tabs-mode: t
// End:
