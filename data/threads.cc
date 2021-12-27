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

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
#include "unconfig.h"
#endif

#include <algorithm>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <mutex>
#include <range/v3/all.hpp>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <chrono>

#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/version.hpp>
#include <boost/date_time.hpp>
#include <boost/dll/import.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filtering_stream.hpp>
using namespace boost::posix_time;
using namespace boost::gregorian;
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
using tcp = net::ip::tcp;         // from <boost/asio/ip/tcp.hpp>

#include <boost/config.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/function.hpp>
#include <boost/timer/timer.hpp>

#include "data/osmobjects.hh"
#include "data/threads.hh"
#include "data/underpass.hh"
#include "hottm.hh"
#include "log.hh"
#include "galaxy/changeset.hh"
#include "galaxy/osmchange.hh"
#include "galaxy/galaxy.hh"
#include "galaxy/replication.hh"
#include "validate/validate.hh"
#include <jemalloc/jemalloc.h>

std::mutex stream_mutex;

using namespace logger;
using namespace galaxy;
using namespace tmdb;

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace threads {

// Starting with this URL, download the file, incrementing
void
startMonitorChangesets(const replication::RemoteURL &inr, const multipolygon_t &poly, const replicatorconfig::ReplicatorConfig &config, thread_pool &pool)
{
#ifdef TIMING_DEBUG
    boost::timer::auto_cpu_timer timer("startMonitorChangesets: took %w seconds\n");
#endif
    // This function is for changesets only!
    assert(inr.frequency == frequency_t::changeset);

    std::shared_ptr<galaxy::QueryGalaxy> ostats = std::make_shared<galaxy::QueryGalaxy>();
    if (!ostats->connect(config.galaxy_db_url)) {
        log_error(_("Could not connect to galaxy DB, aborting monitoring thread!"));
        return;
    }

    osm2pgsql::Osm2Pgsql osm2pgsql(config.osm2pgsql_db_url);

    replication::RemoteURL remote = inr;
    auto planet = std::make_shared<replication::Planet>(remote);

    // Set the catching up target
    auto last_state{config.end_time != not_a_date_time ? planet->fetchData(frequency_t::changeset, config.end_time, config.underpass_db_url) : planet->fetchDataLast(frequency_t::changeset, config.underpass_db_url)};

    if (!last_state->isValid()) {
        log_error(_("Could not get last valid sequence for changesets, aborting monitoring thread!"));
        return;
    }

    // Stores the last valid sequence (+ 1) in the planet
    // we need this value to know when the catching up is
    // probably finished.
    long catchup_pseudo_sequence{last_state->sequence + 1};

    // Timestamp for when the catchup_pseudo_sequence was calculated
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    // Error flag: it is set from the async function on errors
    std::atomic_bool error_flag{false};

    // Catchup delay in seconds
    int catchup_delay{50};

    // Store the last successfully fetched pseudo sequence
    long int max_pseudo_sequence = -1;
    std::mutex max_pseudo_sequence_mutex;

    // List of servers
    auto servers{config.getPlanetServers(frequency_t::changeset)};

    // Processes the changesets asynchronously
    auto processChangesetFunction = [&error_flag, &poly, ostats, &max_pseudo_sequence,
				     &max_pseudo_sequence_mutex](replication::RemoteURL remote,
				     std::vector<replicatorconfig::PlanetServer> servers) {
        int retry{0};
        const int max_retries{4};
#ifdef MEMORY_DEBUG
    size_t sz, active1, active2;
#endif	// JEMALLOC memory debugging

        while (!error_flag) {
#ifdef MEMORY_DEBUG
	    sz = sizeof(size_t);
	    if (mallctl("stats.active", &active1, &sz, NULL, 0) == 0) {
		std::cerr << "startMonitorChangesets: " << active1 << std::endl;
		// thread.allocated,  thread.deallocated
		// thread.prof.name("startMonitorChangesets")
	    }
#endif	// JEMALLOC memory debugging
	    const auto changefile{threadChangeSet(remote, poly, ostats)};
#ifdef MEMORY_DEBUG
	    sz = sizeof(size_t);
	    if (mallctl("stats.active", &active2, &sz, NULL, 0) == 0) {
		std::cerr << "\tstartMonitorChangesets: " << active2 << std::endl;
		// thread.allocated,  thread.deallocated
		// thread.prof.name("startMonitorChangesets")
	    }
	    if (active1 != active2) {
		std::cerr << "\tstartMonitorChangesets is leaking: " << active2-active1 << std::endl;
	    }
#endif	// JEMALLOC memory debugging
            for (const auto &change: std::as_const(changefile->changes)) {
                ostats->applyChange(*change.get());
            }
            if (changefile->download_error) {
                if (retry >= max_retries) {
                    log_error(_("Fatal download error, max retries exceeded for: %1%!"), remote.url);
                    error_flag = true;
                } else {
                    retry++;
                    log_debug(_("Download error (retry %2%) after: %1%!"), remote.url, retry);
                    // Get the next server ...
                    if (servers.size() > 1 && retry < servers.size()) {
                        std::rotate(servers.begin(), servers.begin() + 1, servers.end());
                        const auto first_server{servers.front()};
                        remote.replacePlanet(first_server.domain, first_server.datadir);
                        log_debug(_("Server rotated to: %1%"), remote.domain);
                    } else {
                        // ... or just take a nap
                        log_debug(_("Waiting after download error on: %1%"), remote.url);
                        std::this_thread::sleep_for(std::chrono::seconds{1});
                    }
                }
            } else if (changefile->parse_error) {
                log_error(_("Fatal parse error after: %1%!"), remote.url);
                error_flag = true;
            } else {
                std::scoped_lock{max_pseudo_sequence_mutex};
                const auto sequence{remote.sequence()};
                max_pseudo_sequence = std::max(max_pseudo_sequence, sequence);
                log_debug(_("Changeset processed (%1%): %2%"), sequence, remote.url);
                break;
            }
        }
    };

    // Phase 1: multi-threaded catching up
    while (!error_flag && remote.sequence() <= catchup_pseudo_sequence) {

        pool.push_task(processChangesetFunction, remote, servers);
        if (!error_flag) {
            while (pool.get_tasks_queued() > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        if (servers.size() > 1) {
            std::rotate(servers.begin(), servers.begin() + 1, servers.end());
            const auto first_server{servers.front()};
            remote.replacePlanet(first_server.domain, first_server.datadir);
            log_debug(_("Server rotated to: %1%"), remote.domain);
        }

        remote.Increment();

        // Move the target
        if (max_pseudo_sequence != catchup_pseudo_sequence && config.end_time == not_a_date_time) {
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            catchup_pseudo_sequence = std::max(catchup_pseudo_sequence, catchup_pseudo_sequence + std::chrono::duration_cast<std::chrono::minutes>(now - begin).count() - 5);
        }
    }

    if (error_flag) {
        log_error(_("Could not catch up changesets, monitoring aborted!"));
        return;
    }

    log_debug(_("Catching up completed after: %1%"), remote.url);

    // Phase 2: updates
    const int max_retries{4};
    error_flag = false;

    if (config.end_time == not_a_date_time) {

        // Process changesets
        log_debug(_("Starting changesets updates: %1%"), remote.url);
        int retry = 0;

        // Start monitoring endless loop
        while (!error_flag) {
            const auto changefile{threadChangeSet(remote, poly, ostats)};
            for (const auto &change: std::as_const(changefile->changes)) {
                ostats->applyChange(*change.get());
            }
            if (changefile->download_error) {
                log_debug(_("Download error after: %1%!"), remote.url);
                if (retry > max_retries) {
                    log_error(_("Error while monitoring changeset updates, max retries exceeded for: %1%!"), remote.url);
                    error_flag = true;
                } else {
                    retry++;

                    if (servers.size() > 1) {
                        std::rotate(servers.begin(), servers.begin() + 1, servers.end());
                        const auto first_server{servers.front()};
                        remote.replacePlanet(first_server.domain, first_server.datadir);
                        log_debug(_("Server rotated to: %1%"), remote.domain);
                    }

                    log_debug(_("Waiting %1% seconds for new ChangeSet (sequence: %2%) retry %4%: %3%"), catchup_delay++, remote.sequence(), remote.url, retry);
                    std::this_thread::sleep_for(std::chrono::seconds{catchup_delay++});
                }
            } else if (changefile->parse_error) {
                log_error(_("Fatal parse error after: %1%!"), remote.url);
                error_flag = true;
            } else {
                retry = 0;
                log_debug(_("Changeset processed (%1%): %2%"), remote.sequence(), remote.url);
                remote.Increment();
                catchup_delay = std::max(30, catchup_delay - 1);
                log_debug(_("Waiting %1% seconds for new ChangeSet (sequence: %2%): %3%"), catchup_delay, remote.sequence(), remote.url);
                std::this_thread::sleep_for(std::chrono::seconds{catchup_delay});
            }
        }
    }
}

// Starting with this URL, download the file, incrementing
void
startMonitorChanges(const replication::RemoteURL &inr, const multipolygon_t &poly, const replicatorconfig::ReplicatorConfig &config)
{
#ifdef TIMING_DEBUG
    boost::timer::auto_cpu_timer timer("startMonitorChanges: took %w seconds\n");
#endif
    if (inr.frequency == frequency_t::changeset) {
        log_error(_("Could not start monitoring thread for OSM changes: URL %1% does not appear to be a valid URL for changes!"), inr.url);
        return;
    }

    galaxy::QueryGalaxy ostats;
    if (!ostats.connect(config.galaxy_db_url)) {
        log_error(_("Could not connect to galaxy DB, aborting monitoring thread!"));
        return;
    }

    osm2pgsql::Osm2Pgsql osm2pgsql(config.osm2pgsql_db_url);

    replication::RemoteURL remote = inr;
    auto planet = std::make_shared<replication::Planet>(remote);
    bool mainloop = true;

    std::string plugins;
    if (boost::filesystem::exists("validate/.libs")) {
        plugins = "validate/.libs";
    } else {
        plugins = PKGLIBDIR;
    }
    boost::dll::fs::path lib_path(plugins);
    boost::function<plugin_t> creator;
    try {
        creator =
            boost::dll::import_alias<plugin_t>(lib_path / "libhotosm", "create_plugin", boost::dll::load_mode::append_decorations);
        log_debug(_("Loaded plugin hotosm!"));
    } catch (std::exception &e) {
        log_debug(_("Couldn't load plugin! %1%"), e.what());
        exit(0);
    }
    auto validator = creator();

#ifdef MEMORY_DEBUG
    size_t sz, active1, active2;
#endif	// JEMALLOC memory debugging
    // Process OSM changes
    while (mainloop) {
        std::string file = remote.url + ".osc.gz";
        ptime now = boost::posix_time::microsec_clock::local_time();
#ifdef MEMORY_DEBUG
	sz = sizeof(size_t);
	if (mallctl("thread.allocated", &active1, &sz, NULL, 0) == 0) {
	    std::cerr << "startMonitorChanges: " << active1 << std::endl;
	}
	// malloc_stats_print(NULL, NULL, NULL);
#endif	// JEMALLOC memory debugging	    
        auto osmchange = threadOsmChange(remote, poly, ostats, osm2pgsql, validator);
#ifdef MEMORY_DEBUG
	sz = sizeof(size_t);
	if (mallctl("thread.deallocated", &active2, &sz, NULL, 0) == 0) {
	    std::cerr << "\tstartMonitorChanges: " << active2 << std::endl;
	}
	if (active1 != active2) {
	    std::cerr << "\tstartMonitorChanges is leaking: " << active1 << " : " << active2 << std::endl;
	}
	// malloc_stats_print(NULL, NULL, NULL);
#endif	// JEMALLOC memory debugging

        // FIXME: There is probably a better way to determine when to delay,
        // or when to just keep processing files when catching up.
        boost::posix_time::time_duration delta;
        if (osmchange->changes.size() > 0) {
            delta = now - osmchange->changes.back()->final_entry;
            // log_debug("DELTA2: %1%", (delta.hours()*60) + delta.minutes());
            if (config.end_time != not_a_date_time && osmchange->changes.back()->final_entry >= config.end_time) {
                log_debug(_("Exiting OsmChange because endtime was reached"));
                mainloop = false;
            }
        }

        if (mainloop) {
            if ((delta.hours() * 60) + delta.minutes() <= 1) {
                // log_debug("DELTA: %1%", (delta.hours()*60) + delta.minutes());
                if (remote.frequency == replication::minutely) {
                    std::this_thread::sleep_for(std::chrono::minutes{1});
                } else if (remote.frequency == replication::hourly) {
                    std::this_thread::sleep_for(std::chrono::hours{1});
                } else if (remote.frequency == replication::daily) {
                    std::this_thread::sleep_for(std::chrono::hours{24});
                }
                // planet.reset(new replication::Planet);
            } else {
                log_debug(_("Processed OsmChange: %1%"), remote.url);
            }
            remote.Increment();
        }
    }
}

void
startStateThreads(const std::string &base, const std::string &file)
{
    // std::map<std::string, std::thread> thread_pool;

    //return;                     // FIXME:

    // boost::system::error_code ec;
    // underpass::Underpass under;
    // under.connect();
    // auto planet = std::make_shared<replication::Planet>();
    // planet->connectServer();

    // #if 1
    //     for (auto it = std::begin(files); it != std::end(files); ++it) {
    //         // There are no state,txt file before this directory
    //         // https://planet.openstreetmap.org/replication/changesets/002/008

    //         //  state = [planet](const std::string &path)->bool {
    //         std::string path = base + it->substr(0, 3);
    //         std::shared_ptr<replication::StateFile> state = threadStateFile(planet->stream, path + ".state.txt");
    //         if (!state->path.empty()) {
    //             under.writeState(*state);
    //             state->dump();
    //         } else {
    //             std::cerr << "ERROR: No StateFile returned: " << path << std::endl;
    //             // planet.reset(new replication::Planet);
    //             // planet.reset(new replication::Planet());
    //             std::this_thread::sleep_for(std::chrono::seconds{1});
    //             state = threadStateFile(planet->stream, path + ".state.txt");
    //             if (!state->path.empty()) {
    //                 under.writeState(*state);
    //                 state->dump();
    //             }
    //         }
    //     }
    // #else
    //     // boost::asio::thread_pool pool(20);
    //     boost::asio::thread_pool pool(/* std::thread::hardware_concurrency() */ );

    //     // Note this uses ranges, which only got added in C++20, so
    //     // for now use the ranges-v3 library, which is the implementation.
    //     // The planet server drops the network connection after 111
    //     // GET requests, so break the 1000 strings into smaller chunks
    //     // 144, 160, 176, 192, 208, 224
    //     auto rng  = files | ranges::views::chunk(200);

    //     // underpass::Underpass under;
    //     // under.connect();
    //     for (auto cit = std::begin(rng); cit != std::end(rng); ++cit) {
    //         log_debug(_("Chunk data: %1%"), *cit));
    //         for (auto it = std::begin(*cit); it != std::end(*cit); ++it) {
    //             // There are no state,txt file before this directory
    //             // https://planet.openstreetmap.org/replication/changesets/002/008
    //             if (boost::filesystem::extension(*it) != ".txt") {
    //                 continue;
    //             }
    //             std::string subpath = base + it->substr(0, it->size() - 10);
    //             auto exists = under.getState(subpath);
    //             if (!exists->path.empty()) {
    //                 log_debug(_("Already stored: %1%"), subpath);
    //                 continue;
    //             }
    //             // Add a thread to the pool for this file
    //             if (!it->size() <= 1) {
    // #ifdef USE_MULTI_LOADER
    //                 boost::asio::post(pool, [subpath, state]{state(subpath);});
    // #else
    //                 auto state = threadStateFile(planet->stream, base + *it);
    //                 if (!state->path.empty()) {
    //                     // under.writeState(*state);
    //                     state->dump();
    //                     continue;
    //                 } else {
    //                     log_error(_("No StateFile returned"));
    //                 }
    // #endif
    //             }
    //         }
    //         // Don't hit the server too hard while testing, it's not polite
    //         // std::this_thread::sleep_for(std::chrono::seconds{1});
    //         planet->disconnectServer();
    //         planet.reset(new replication::Planet);
    //     }
    // #ifdef USE_MULTI_LOADER
    //     pool.join();
    // #endif
    //     planet->ioc.reset();
    // #endif
    //     // planet->stream.socket().shutdown(tcp::socket::shutdown_both, ec);
}

// This thread get started for every osmChange file
std::shared_ptr<osmchange::OsmChangeFile>
threadOsmChange(const replication::RemoteURL &remote, const multipolygon_t &poly, galaxy::QueryGalaxy &ostats,
                osm2pgsql::Osm2Pgsql &o2pgsql, std::shared_ptr<Validate> &plugin)
{
    // galaxy::QueryGalaxy ostats;
    std::vector<std::string> result;
    auto osmchanges = std::make_shared<osmchange::OsmChangeFile>();
#ifdef TIMING_DEBUG
    boost::timer::auto_cpu_timer timer("threadOsmChange: took %w seconds\n");
#endif

    auto data = std::make_shared<std::vector<unsigned char>>();
    // If the file is stored on disk, read it in instead of downloading
    if (boost::filesystem::exists(remote.filespec)) {
        log_debug(_("Reading osmChange: %1%"), remote.filespec);
        // Since we want to read in the entire file so it can be
        // decompressed, blow off C++ streaming and just load the
        // entire thing.
        int size = boost::filesystem::file_size(remote.filespec);
        data->reserve(size);
        data->resize(size);
        int fd = open(remote.filespec.c_str(), O_RDONLY);
        char *buf = new char[size];
        //memset(buf, 0, size);
        read(fd, buf, size);
        // FIXME: it would be nice to avoid this copy
        std::copy(buf, buf + size, data->begin());
        close(fd);
        free(buf);
    } else {
        log_debug(_("Downloading osmChange: %1%"), remote.url);
        replication::Planet planet(remote);
        data = planet.downloadFile(remote.url);
    }
    if (data->size() == 0) {
        log_error(_("osmChange file not found: %1% %2%"), remote.url, ".osc.gz");
        return osmchanges;
    } else {
#ifdef USE_CACHE
        if (!boost::filesystem::exists(remote.destdir)) {
            boost::filesystem::create_directories(remote.destdir);
        }
        std::ofstream myfile;
        myfile.open(remote.filespec, std::ios::binary);
        myfile.write(reinterpret_cast<char *>(data->data()), data->size());
        myfile.close();
#endif
        try {

            std::istringstream changes_xml;

            // Scope to deallocate buffers
            {
                boost::iostreams::filtering_streambuf<boost::iostreams::input> inbuf;
                inbuf.push(boost::iostreams::gzip_decompressor());
                boost::iostreams::array_source arrs{reinterpret_cast<char const *>(data->data()), data->size()};
                inbuf.push(arrs);
                std::istream instream(&inbuf);
                changes_xml.str(std::string{std::istreambuf_iterator<char>(instream), {}});
            }

            try {
                osmchanges->readXML(changes_xml);

            } catch (std::exception &e) {
                log_error(_("Couldn't parse: %1%"), remote.url);
                std::cerr << e.what() << std::endl;
            }

        } catch (std::exception &e) {
            log_error(_("%1% is corrupted!"), remote.url);
            std::cerr << e.what() << std::endl;
        }
    }

#if 0
    // Apply the changes to the database
    underpass::Underpass under;
    under.connect();
    replication::StateFile state;
    for (auto it = std::begin(osmchanges.changes); it != std::end(osmchanges.changes); ++it) {
        //state.created_at = it->created_at;
        //state.closed_at = it->closed_at;
        state.frequency = replication::changeset;
        state.path = file;
        under.writeState(state);
    }
#endif

    for (auto cit = std::begin(osmchanges->changes); cit != std::end(osmchanges->changes); ++cit) {
        osmchange::OsmChange *change = cit->get();
        // change->dump();
        for (auto nit = std::begin(change->nodes); nit != std::end(change->nodes); ++nit) {
            osmobjects::OsmNode *node = nit->get();
        }
        for (auto wit = std::begin(change->ways); wit != std::end(change->ways); ++wit) {
            osmobjects::OsmWay *way = wit->get();
        }
    }
    osmchanges->areaFilter(poly);

    if (o2pgsql.isOpen()) {
        // o2pgsql.updateDatabase(osmchanges);
    }

    // These stats are for the entire file
    auto stats = osmchanges->collectStats(poly);
    for (auto it = std::begin(*stats); it != std::end(*stats); ++it) {
        if (it->second->added.size() == 0 && it->second->modified.size() == 0) {
            continue;
        }
        // it->second->dump();
        ostats.applyChange(*it->second);
    }

    // Delete existing entries in the validation table to remove features that have been fixed
    for (auto it = std::begin(osmchanges->changes); it != std::end(osmchanges->changes); ++it) {
        OsmChange *change = it->get();
        for (auto wit = std::begin(change->ways); wit != std::end(change->ways); ++wit) {
	    osmobjects::OsmWay *way = wit->get();
	    ostats.updateValidation(way->id);
	}
        for (auto nit = std::begin(change->nodes); nit != std::end(change->nodes); ++nit) {
            osmobjects::OsmNode *node = nit->get();
	    ostats.updateValidation(node->id);
	}
    }

    auto nodeval = osmchanges->validateNodes(poly, plugin);
    // std::cerr << "SIZE " << nodeval->size() << std::endl;
    for (auto it = nodeval->begin(); it != nodeval->end(); ++it) {
        ostats.applyChange(*it->get());
    }
    auto wayval = osmchanges->validateWays(poly, plugin);
    // std::cerr << "SIZE " << wayval->size() << std::endl;
    for (auto it = wayval->begin(); it != wayval->end(); ++it) {
        ostats.applyChange(*it->get());
    }

    return osmchanges;
}

// This parses the changeset file into changesets
std::unique_ptr<changeset::ChangeSetFile>
threadChangeSet(const replication::RemoteURL &remote, const multipolygon_t &poly, std::shared_ptr<galaxy::QueryGalaxy> ostats)
{
#ifdef TIMING_DEBUG
    boost::timer::auto_cpu_timer timer("threadChangeSet: took %w seconds\n");
#endif
    auto changeset = std::make_unique<changeset::ChangeSetFile>();
    auto data = std::make_shared<std::vector<unsigned char>>();

    if (boost::filesystem::exists(remote.filespec)) {
        log_debug(_("Reading ChangeSet: %1%"), remote.filespec);
        // Since we want to read in the entire file so it can be
        // decompressed, blow off C++ streaming and just load the
        // entire thing.
        int size = boost::filesystem::file_size(remote.filespec);
        data->reserve(size);
        data->resize(size);
        int fd = open(remote.filespec.c_str(), O_RDONLY);
        char *buf = new char[size];
        //memset(buf, 0, size);
        read(fd, buf, size);
        // FIXME: it would be nice to avoid this copy
        std::copy(buf, buf + size, data->begin());
        close(fd);
    } else {
        log_debug(_("Downloading ChangeSet: %1%"), remote.url);
        try {
            replication::Planet planet(remote);
            data = planet.downloadFile(remote.url);
        } catch (const std::exception &ex) {
            log_debug(_("Download error for %1%: %2%"), remote.url, ex.what());
        }
    }
    if (data->size() == 0) {
        log_error(_("ChangeSet file not found: %1%"), remote.url);
        changeset->download_error = true;
    } else {
        //result.set_value(true);
        // XML parsers expect every line to have a newline, including the end of file
#ifdef USE_CACHE
        if (!boost::filesystem::exists(remote.destdir)) {
            boost::filesystem::create_directories(remote.destdir);
        }
        std::ofstream myfile;
        myfile.open(remote.filespec, std::ios::binary);
        myfile.write(reinterpret_cast<char *>(data->data()), data->size());
        myfile.close();
#endif
        //data->push_back('\n');
        try {
            boost::iostreams::filtering_streambuf<boost::iostreams::input> inbuf;
            inbuf.push(boost::iostreams::gzip_decompressor());
            // data->push_back('\n');
            boost::iostreams::array_source arrs{reinterpret_cast<char const *>(data->data()), data->size()};
            inbuf.push(arrs);
            std::istream instream(&inbuf);
            try {
                changeset->readXML(instream);
            } catch (std::exception &e) {
                log_error(_("Couldn't parse: %1% %2%"), remote.url, e.what());
                changeset->parse_error = true;
            }
            // change.readXML(instream);
        } catch (std::exception &e) {
            log_error(_("%1% is corrupted!"), remote.url);
            changeset->parse_error = true;
        }
    }

    // Note: closed_at might be not_a_date_time because some records in changesets XML miss that attribute!
    // log_debug(_("Closed At:  %1%"), to_simple_string(changeset->last_closed_at));

    changeset->areaFilter(poly);

    return changeset;
}

// This updates the calculated fields in the raw_changesets table, based on
// the data in the OSM stats database.
void
threadStatistics(const std::string &database, ptime &timestamp)
{
    //galaxy::QueryGalaxy ostats;
    replication::Replication repl;
}

// Updates the states table in the Underpass database
std::shared_ptr<replication::StateFile>
threadStateFile(ssl::stream<tcp::socket> &stream, const std::string &file)
{
    std::string server;

    std::vector<std::string> result;
    boost::split(result, file, boost::is_any_of("/"));
    server = result[2];

    // This buffer is used for reading and must be persistant
    boost::beast::flat_buffer buffer;
    boost::beast::error_code ec;

    // Set up an HTTP GET request message
    http::request<http::string_body> req{http::verb::get, file, 11};

    req.keep_alive();
    req.set(http::field::host, server);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    log_debug(_("(%1%)Downloading %2%"), std::this_thread::get_id(), file);

    // Stays locked till the function exits
    const std::lock_guard<std::mutex> lock(stream_mutex);

    // Send the HTTP request to the remote host
    // std::lock_guard<std::mutex> guard(stream_mutex);
    boost::beast::http::response_parser<http::string_body> parser;

    http::write(stream, req);
    boost::beast::http::read(stream, buffer, parser, ec);
    if (ec == http::error::partial_message) {
        log_network(_("ERROR: partial read: %1%"), ec.message());
        std::this_thread::yield();
        http::write(stream, req);
        boost::beast::http::read(stream, buffer, parser, ec);
        // Give the network a chance to recover
        // std::this_thread::sleep_for(std::chrono::seconds{1});
        //return std::make_shared<replication::StateFile>();
    }
    if (ec == http::error::end_of_stream) {
        log_error(_("end of stream read failed: %1%"), ec.message());
        // Give the network a chance to recover
        // stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        return std::make_shared<replication::StateFile>();
    } else if (ec) {
        log_network(_("ERROR: stream read failed: %1%"), ec.message());
        return std::make_shared<replication::StateFile>();
    }
    if (parser.get().result() == boost::beast::http::status::not_found) {
        // continue;
    }

    // File never downloaded, return empty
    if (parser.get().body().size() < 10) {
        log_error(_("failed to download: %1%"), file);
        return std::make_shared<replication::StateFile>();
    }

    //const std::lock_guard<std::mutex> unlock(stream_mutex);
    auto data = std::make_shared<std::vector<unsigned char>>();
    for (auto body = std::begin(parser.get().body()); body != std::end(parser.get().body()); ++body) {
        data->push_back((unsigned char)*body);
    }
    if (data->size() == 0) {
        log_error(_("StateFile not found: %1%"), file);
        return std::make_shared<replication::StateFile>();
    } else {
        std::string tmp(reinterpret_cast<const char *>(data->data()));
        auto state = std::make_shared<replication::StateFile>(tmp, true);
        if (!file.empty()) {
            state->path = file.substr(0, file.size() - 10);
        }
        return state;
    }
}

void
threadTMUsersSync(std::atomic<bool> &tmUserSyncIsActive, const replicatorconfig::ReplicatorConfig &config)
{
    // There is a lot of DB URI manipulations in this program, if the URL
    // contains a plain hostname we need to add a database name too
    // FIXME: handle all DB URIs in a consistent and documented way
    auto galaxyDbUrlWithDbName{config.galaxy_db_url};
    if (config.galaxy_db_url.find('/') == std::string::npos) {
        galaxyDbUrlWithDbName.append("/galaxy");
    }
    auto galaxy{QueryGalaxy()};
    // Connection errors are fatal: exit!
    if (!galaxy.connect(galaxyDbUrlWithDbName)) {
        log_error("ERROR: couldn't connect to OSM Stats Underpass server: %1%!", galaxyDbUrlWithDbName);
        return;
    }

    TaskingManager taskingManager;
    // FIXME: handle all DB URIs in a consistent and documented way
    auto tmDbUrlWithDbName{config.taskingmanager_db_url};
    if (config.taskingmanager_db_url.find('/') == std::string::npos) {
        tmDbUrlWithDbName.append("/taskingmanager");
    }

    if (!taskingManager.connect(tmDbUrlWithDbName)) {
        log_error("ERROR: couldn't connect to Tasking Manager server: %1%!", tmDbUrlWithDbName);
        return;
    }

    const auto tmusersfrequency{config.taskingmanager_users_update_frequency};

    do {

        auto start{std::chrono::system_clock::now()};
        const auto users{taskingManager.getUsers()};
        // Sync and delete missing
        const auto results{galaxy.syncUsers(users, true)};
        auto end{std::chrono::system_clock::now()};
        auto elapsed{std::chrono::duration_cast<std::chrono::seconds>(end - start)};

        log_debug("Users sync TM->OS executed in %1% seconds.", elapsed.count());
        log_debug("Users created: %1%, updated: %2%, deleted: %3%", results.created, results.updated, results.deleted);

        if (tmusersfrequency > 0) {
            if (elapsed.count() < tmusersfrequency) {
                log_debug("Users sync TM->OS sleeping for %1% seconds...",
                          std::chrono::seconds(tmusersfrequency - elapsed.count()).count());
                std::this_thread::sleep_for(std::chrono::seconds(tmusersfrequency - elapsed.count()));
            }
        }

    } while (tmusersfrequency > 0 && tmUserSyncIsActive);
};

} // namespace threads

// local Variables:
// mode: C++
// indent-tabs-mode: t
// End:
