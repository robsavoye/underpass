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
// #include <zlib.h>
#include "gunzip.hh"

#include <osmium/io/any_input.hpp>
#include <osmium/builder/osm_object_builder.hpp>
#include <osmium/handler.hpp>
#include <osmium/visitor.hpp>
#include <osmium/io/any_output.hpp>
#include <glibmm/convert.h> //For Glib::ConvertError

#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include "hotosm.hh"
#include "osmstats/changeset.hh"

namespace changeset {

/// parse the two state file for a replication file, from
/// disk or memory.

// There are two types of state files with of course different
// formats for the same basic data. The simplest one is for
// changesets, which looks like this:
// ---
// last_run: 2020-10-08 22:30:01.737719000 +00:00
// sequence: 4139992
//
// The other format is used for minutely change files, and
// has mnore fields. For now, only the timestamp and sequence
// number is stored. It looks like this:
// #Fri Oct 09 10:03:04 UTC 2020
// sequenceNumber=4230996
// txnMaxQueried=3083073477
// txnActiveList=
// txnReadyList=
// txnMax=3083073477
// timestamp=2020-10-09T10\:03\:02Z
//
// State files are used to know where to start downloading files
StateFile::StateFile(const std::string &file, bool memory)
{
    std::string line;
    std::ifstream state;
    std::stringstream ss;

    // It's a disk file, so read it in.
    if (!memory) {
        try {
            state.open(file, std::ifstream::in);
        }
        catch(std::exception& e) {
            std::cout << "ERROR opening " << file << std::endl;
            std::cout << e.what() << std::endl;
            // return false;
        }
        // For a disk file, none of the state files appears to be larger than
        // 70 bytes, so read the whole thing into memory without
        // any iostream buffering.
        std::filesystem::path path = file;
        int size = std::filesystem::file_size(path);
        char *buffer = new char[size];
        state.read(buffer, size);
        ss << buffer;
        // FIXME: We do it this way to save lots of extra buffering
        // ss.rdbuf()->pubsetbuf(&buffer[0], size);
    } else {
        // It's in memory
        ss << file;
    }

    // Get the first line
    std::getline(ss, line, '\n');

    // This is a changeset state.txt file
    if (line == "---") {
        // Second line is the last_run timestamp
        std::getline(ss, line, '\n');
        // The timestamp is the second field
        std::size_t pos = line.find(" ");
        // 2020-10-08 22:30:01.737719000 +00:00
        timestamp = time_from_string(line.substr(pos+1));

        // Third and last line is the sequence number
        std::getline(ss, line, '\n');
        pos = line.find(" ");
        // The sequence is the second field
        sequence = std::stol(line.substr(pos+1));
        // This is a change file state.txt file
    } else {
        std::getline(ss, line, '\n'); // sequenceNumber
        std::size_t pos = line.find("=");
        sequence= std::stol(line.substr(pos+1));
        std::getline(ss, line, '\n'); // txnMaxQueried
        std::getline(ss, line, '\n'); // txnActiveList
        std::getline(ss, line, '\n'); // txnReadyList
        std::getline(ss, line, '\n'); // txnMax
        std::getline(ss, line, '\n');
        pos = line.find("=");
        // The time stamp is in ISO format, ie... 2020-10-09T10\:03\:02
        // But we have to unescape the colon or boost chokes.
        std::string tmp = line.substr(pos+1);
        pos = tmp.find('\\', pos+1);
        std::string tstamp = tmp.substr(0, pos); // get the date and the hour
        tstamp += tmp.substr(pos+1, 3); // get minutes
        pos = tmp.find('\\', pos+1);
        tstamp += tmp.substr(pos+1, 3); // get seconds
        timestamp = from_iso_extended_string(tstamp);
    }

    state.close();
}

// Read a changeset file from disk or memory
bool
ChangeSetFile::readChanges(const std::string &file, bool memory)
{
    std::ifstream change;
    int size = 0;

    // It's a disk file, so read it in.
    unsigned char *buffer;
    if (!memory) {
        try {
            change.open(file,  std::ifstream::in |  std::ifstream::binary);
        }
        catch(std::exception& e) {
            std::cout << "ERROR opening " << file << std::endl;
            std::cout << e.what() << std::endl;
            // return false;
        }
        // For a disk file, none of the changeset files appears to be larger
        // than a few kilobytes, so read the whole thing into memory without
        // any iostream buffering.
        std::filesystem::path path = file;
        size = std::filesystem::file_size(path);
        buffer = new unsigned char[size];
        change.read((char *)buffer, size);
    }

    // It's a gzipped XML file, so decompress it for parsing.
    unsigned char outbuffer[10000];
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    // This is needed for gzip
    inflateInit2(&strm, 16+MAX_WBITS);
    strm.avail_in = size;
    strm.next_in = buffer;
    // FIXME: This should be calculated, not hardcoded to 3M
    strm.avail_out = 3000000;
    strm.next_out = outbuffer;

    int ret = inflate( &strm, Z_FINISH );

    inflateEnd( &strm );
    //. Terminate the data for printing
    outbuffer[strm.total_out] = 0;
    // FIXME: debug print so we know it's working
    std::cout << outbuffer << std::endl;

    readXML((char *)outbuffer);
    change.close();
}

void ChangeSetFile::on_start_document()
{
  std::cout << "on_start_document()" << std::endl;
}

void
ChangeSetFile::on_end_document()
{
    std::cout << "on_end_document()" << std::endl;
}

void
ChangeSetFile::on_start_element(const Glib::ustring& name,
                                   const AttributeList& attributes)
{
    std::cout << "node name=" << name << std::endl;

    // Print attributes:
    for(const auto& attr_pair : attributes) {
        try {
            std::cout << "  Attribute name=" <<  attr_pair.name << std::endl;
        }
        catch(const Glib::ConvertError& ex) {
            std::cerr << "ChangeSetFile::on_start_element(): Exception caught while converting name for std::cout: " << ex.what() << std::endl;
        }

        try {
            std::cout << "    , value= " <<  attr_pair.value << std::endl;
        }
        catch(const Glib::ConvertError& ex) {
            std::cerr << "ChangeSetFile::on_start_element(): Exception caught while converting value for std::cout: " << ex.what() << std::endl;
        }
    }
}

void
ChangeSetFile::on_end_element(const Glib::ustring& /* name */)
{
    std::cout << "on_end_element()" << std::endl;
}

void
ChangeSetFile::on_characters(const Glib::ustring& text)
{
    try {
        std::cout << "on_characters(): " << text << std::endl;
    }
    catch(const Glib::ConvertError& ex) {
        std::cerr << "ChangeSetFile::on_characters(): Exception caught while converting text for std::cout: " << ex.what() << std::endl;
    }
}

void
ChangeSetFile::on_comment(const Glib::ustring& text)
{
    try {
        std::cout << "on_comment(): " << text << std::endl;
    }
    catch(const Glib::ConvertError& ex) {
        std::cerr << "ChangeSetFile::on_comment(): Exception caught while converting text for std::cout: " << ex.what() << std::endl;
    }
}

void
ChangeSetFile::on_warning(const Glib::ustring& text)
{
    try {
        std::cout << "on_warning(): " << text << std::endl;
    }
    catch(const Glib::ConvertError& ex) {
        std::cerr << "ChangeSetFile::on_warning(): Exception caught while converting text for std::cout: " << ex.what() << std::endl;
    }
}

void
ChangeSetFile::on_error(const Glib::ustring& text)
{
    try {
        std::cout << "on_error(): " << text << std::endl;
    }
    catch(const Glib::ConvertError& ex) {
        std::cerr << "ChangeSetFile::on_error(): Exception caught while converting text for std::cout: " << ex.what() << std::endl;
    }
}

void
ChangeSetFile::on_fatal_error(const Glib::ustring& text)
{
    try {
        std::cout << "on_fatal_error(): " << text << std::endl;
    }
    catch(const Glib::ConvertError& ex) {
        std::cerr << "ChangeSetFile::on_characters(): Exception caught while converting value for std::cout: " << ex.what() << std::endl;
    }
}

bool
ChangeSetFile::readXML(const std::string xml)
{
    // changes[] = ChangeSet();
    try {
        ChangeSetFile parser;
        parser.set_substitute_entities(true);
        parser.parse_memory(xml);
    }
    catch(const xmlpp::exception& ex) {
        std::cerr << "libxml++ exception: " << ex.what() << std::endl;
        int return_code = EXIT_FAILURE;
    }
}

}       // EOF changeset

