//
// Copyright (c) 2020, 2021, 2022, 2023, 2024 Humanitarian OpenStreetMap Team
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

/// \file yaml.cc
/// \brief Simple YAML file reader.
///
/// Read in a YAML config file and create a nested data structure so it can be accessed.

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
# include "unconfig.h"
#endif

#include "yaml.hh"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace yaml {

void Yaml::read(const std::string &fspec) {
    std::ifstream yaml;
    std::string line;
    filespec = fspec;

    if (!std::filesystem::exists(filespec)) {
        std::cout << "Couldn't open " << filespec << std::endl;
        return;
    }
    yaml.open(filespec,  std::ifstream::in);

    this->root = Node();
    while (getline(yaml, line)) {
        if (line.size() > 0 && line[0] != '#') {
            auto lineinfo = this->process_line(line);
            auto line = lineinfo.first;
            auto index = lineinfo.second;
            auto node = Node();
            this->clean(line);
            node.value = line;
            add_node(node, index, root);
        }
    }
}

void Yaml::add_node(Node &node, int index, Node &parent, int depth) {
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

void Yaml::clean(std::string &line) {
    if (line.front() == '-') {
        line.erase(0,2);
    }
    if (line.back() == ':') {
        line.pop_back();
    }
}

std::pair<std::string, int> Yaml::process_line(std::string line ) {
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

std::string Yaml::scan_ident(std::string line) {
    std::string firstChar = line.substr(0, 1);
    const char *c = firstChar.c_str();
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

void Yaml::dump() {
    this->root.dump();
}

Node Yaml::get(std::string key) {
    return this->root.get(key);
}

bool Yaml::contains_key(std::string key) {
    return this->root.contains_key(key);
}

bool Yaml::contains_value(std::string key, std::string value) {
    return this->root.contains_value(key, value);
}

Node::Node() {};
Node::~Node() {};

Node Node::get(std::string key) {
    auto result = Node();
    this->get(key, result);
    return result;
}

std::string Node::get_value(std::string key) {
    auto result = Node();
    this->get(key, result);
    if (result.children.size() > 0) {
        return result.children.front().value;
    }
    return "";
}

std::vector<std::string> Node::get_values(std::string key) {
    auto result = Node();
    this->get(key, result);
    std::vector<std::string> values;
    if (result.children.size() > 0) {
        for (auto it = result.children.begin(); it != result.children.end(); ++it) {
            values.push_back(it->value);
        }
    }
    return values;
}

void Node::get(std::string key, Node &result) {
    for (auto it = std::begin(this->children); it != std::end(this->children); ++it) {
        if (it->value == key) {
            result = *it;
            return;
        }
        if (it->children.size() > 0) {
            it->get(key, result);
        }
    }
}

bool Node::contains_key(std::string key) {
    bool result = false;
    this->contains_key(key, result);
    return result;
}

void Node::contains_key(std::string key, bool &result) {
    for (auto it = std::begin(this->children); it != std::end(this->children); ++it) {
        if (it->value == key) {
            result = true;
            return;
        }
        if (it->children.size() > 0) {
            it->contains_key(key, result);
        }
    }
}

bool Node::contains_value(std::string key, std::string value) {
    bool result = false;
    this->contains_value(key, value, result);
    return result;
}

void Node::contains_value(std::string key, std::string value, bool &result) {
    for (auto it = std::begin(this->children); it != std::end(this->children); ++it) {
        if (it->value == key) {
            for (auto itk = std::begin(it->children); itk != std::end(it->children); ++itk) {
                if (itk->value == value) {
                    result = true;
                    return;
                }
            }
            return;
        }
        if (it->children.size() > 0) {
            it->contains_value(key, value, result);
        }
    }
}

void Node::dump() {
    for (auto it = std::begin(this->children); it != std::end(this->children); ++it) {
        if (it->children.size() > 0) {
            it->dump();
        }
    }
}

} // EOF yaml namespace

// Local Variables:
// mode: C++
// indent-tabs-mode: nil
// End:
