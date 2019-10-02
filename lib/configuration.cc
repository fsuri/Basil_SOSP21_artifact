// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * configuration.cc:
 *   Representation of a replica group configuration, i.e. the number
 *   and list of replicas in the group
 *
 * Copyright 2013 Dan R. K. Ports  <drkp@cs.washington.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/message.h"

#include <iostream>
#include <cstring>
#include <stdexcept>
#include <tuple>

namespace transport {

ReplicaAddress::ReplicaAddress(const string &host, const string &port)
    : host(host), port(port)
{

}

bool
ReplicaAddress::operator==(const ReplicaAddress &other) const {
    return ((host == other.host) &&
            (port == other.port));
}

bool
ReplicaAddress::operator<(const ReplicaAddress &other) const {
    auto this_t = std::forward_as_tuple(host, port);
    auto other_t = std::forward_as_tuple(other.host, other.port);
    return this_t < other_t;
}

Configuration::Configuration(const Configuration &c)
    : n(c.n), f(c.f), replicas(c.replicas), hasMulticast(c.hasMulticast)
{
    multicastAddress = NULL;
    if (hasMulticast) {
        multicastAddress = new ReplicaAddress(*c.multicastAddress);
    }
}

Configuration::Configuration(const Configuration &c, bool is_janus)
    : g(c.g), n(c.n), f(c.f), g_replicas(c.g_replicas), hasMulticast(c.hasMulticast),
      hasFC(c.hasFC), interfaces(c.interfaces)
{
    multicastAddress = NULL;
    if (hasMulticast) {
        multicastAddress = new ReplicaAddress(*c.multicastAddress);
    }
    fcAddress = NULL;
    if (hasFC) {
        fcAddress = new ReplicaAddress(*c.fcAddress);
    }
}

Configuration::Configuration(int n, int f,
                             std::vector<ReplicaAddress> replicas,
                             ReplicaAddress *multicastAddress)
    : n(n), f(f), replicas(replicas)
{
    if (multicastAddress) {
        hasMulticast = true;
        this->multicastAddress =
            new ReplicaAddress(*multicastAddress);
    } else {
        hasMulticast = false;
        multicastAddress = NULL;
    }
}

Configuration::Configuration(int g, int n, int f,
                             std::map<int, std::vector<ReplicaAddress> > g_replicas,
                             ReplicaAddress *multicastAddress,
                             ReplicaAddress *fcAddress,
                             std::map<int, std::vector<std::string> > interfaces)
    : g(g), n(n), f(f), g_replicas(g_replicas), interfaces(interfaces)
{
    if (multicastAddress) {
        hasMulticast = true;
        this->multicastAddress =
            new ReplicaAddress(*multicastAddress);
    } else {
        hasMulticast = false;
        multicastAddress = NULL;
    }

    if (fcAddress) {
        hasFC = true;
        this->fcAddress =
            new ReplicaAddress(*fcAddress);
    } else {
        hasFC = false;
        fcAddress = NULL;
    }
}

Configuration::Configuration(std::ifstream &file)
{
    f = -1;
    hasMulticast = false;
    multicastAddress = NULL;

    while (!file.eof()) {
        // Read a line
        string line;
        getline(file, line);
        // Ignore comments
        if ((line.size() == 0) || (line[0] == '#')) {
            continue;
        }

        // Get the command
        unsigned int t1 = line.find_first_of(" \t");
        string cmd = line.substr(0, t1);

        if (strcasecmp(cmd.c_str(), "f") == 0) {
            unsigned int t2 = line.find_first_not_of(" \t", t1);
            if (t2 == string::npos) {
                Panic ("'f' configuration line requires an argument");
            }

            try {
                f = stoul(line.substr(t2, string::npos));
            } catch (std::invalid_argument& ia) {
                Panic("Invalid argument to 'f' configuration line");
            }
        } else if (strcasecmp(cmd.c_str(), "replica") == 0) {
            unsigned int t2 = line.find_first_not_of(" \t", t1);
            if (t2 == string::npos) {
                Panic ("'replica' configuration line requires an argument");
            }

            unsigned int t3 = line.find_first_of(":", t2);
            if (t3 == string::npos) {
                Panic("Configuration line format: 'replica host:port'");
            }

            string host = line.substr(t2, t3-t2);
            string port = line.substr(t3+1, string::npos);

            replicas.push_back(ReplicaAddress(host, port));
        } else if (strcasecmp(cmd.c_str(), "multicast") == 0) {
            unsigned int t2 = line.find_first_not_of(" \t", t1);
            if (t2 == string::npos) {
                Panic ("'multicast' configuration line requires an argument");
            }

            unsigned int t3 = line.find_first_of(":", t2);
            if (t3 == string::npos) {
                Panic("Configuration line format: 'replica host:port'");
            }

            string host = line.substr(t2, t3-t2);
            string port = line.substr(t3+1, string::npos);

            multicastAddress = new ReplicaAddress(host, port);
            hasMulticast = true;
        } else {
            Panic("Unknown configuration directive: %s", cmd.c_str());
        }
    }

    n = replicas.size();
    if (n == 0) {
        Panic("Configuration did not specify any replicas");
    }

    if (f == -1) {
        Panic("Configuration did not specify a 'f' parameter");
    }
}

// HACK: add a bool kek
Configuration::Configuration(std::ifstream &file, bool is_janus)
{
    f = -1;
    hasMulticast = false;
    multicastAddress = NULL;
    hasFC = false;
    fcAddress = NULL;
    int group = -1;

    while (!file.eof()) {
        // Read a line
        string line;
        getline(file, line);;

        // Ignore comments
        if ((line.size() == 0) || (line[0] == '#')) {
            continue;
        }

        // Get the command
        // This is pretty horrible, but C++ does promise that &line[0]
        // is going to be a mutable contiguous buffer...
        char *cmd = strtok(&line[0], " \t");

        if (strcasecmp(cmd, "f") == 0) {
            char *arg = strtok(NULL, " \t");
            if (!arg) {
                Panic ("'f' configuration line requires an argument");
            }
            char *strtolPtr;
            f = strtoul(arg, &strtolPtr, 0);
            if ((*arg == '\0') || (*strtolPtr != '\0')) {
                Panic("Invalid argument to 'f' configuration line");
            }
        } else if (strcasecmp(cmd, "group") == 0) {
            group++;
        } else if (strcasecmp(cmd, "replica") == 0) {
            if (group < 0) {
                group = 0;
            }

            char *arg = strtok(NULL, " \t");
            if (!arg) {
                Panic ("'replica' configuration line requires an argument");
            }

            char *host = strtok(arg, ":");
            char *port = strtok(NULL, ":");
            char *interface = strtok(NULL, "");

            if (!host || !port) {
                Panic("Configuration line format: 'replica group host:port'");
            }

            g_replicas[group].push_back(ReplicaAddress(string(host), string(port)));
            if (interface != nullptr) {
                interfaces[group].push_back(string(interface));
            } else {
                interfaces[group].push_back(string());
            }
        } else if (strcasecmp(cmd, "multicast") == 0) {
            char *arg = strtok(NULL, " \t");
            if (!arg) {
                Panic ("'multicast' configuration line requires an argument");
            }

            char *host = strtok(arg, ":");
            char *port = strtok(NULL, "");

            if (!host || !port) {
                Panic("Configuration line format: 'multicast host:port'");
            }

            multicastAddress = new ReplicaAddress(string(host),
                                                  string(port));
            hasMulticast = true;
        } else if (strcasecmp(cmd, "fc") == 0) {
            char *arg = strtok(NULL, " \t");
            if (!arg) {
                Panic ("'fc' configuration line requires an argument");
            }

            char *host = strtok(arg, ":");
            char *port = strtok(NULL, "");

            if (!host || !port) {
                Panic("Configuration line format: 'fc host:port'");
            }

            fcAddress = new ReplicaAddress(string(host),
                                           string(port));
            hasFC = true;
        } else {
            Panic("Unknown configuration directive: %s", cmd);
        }
    }

    g = g_replicas.size();

    if (g == 0) {
        Panic("Configuration did not specify any groups");
    }

    n = g_replicas[0].size();

    for (auto &kv : g_replicas) {
        if (kv.second.size() != (size_t)n) {
            Panic("All groups must contain the same number of replicas.");
        }
    }

    if (n == 0) {
        Panic("Configuration did not specify any replicas");
    }

    if (f == -1) {
        Panic("Configuration did not specify a 'f' parameter");
    }
}


Configuration::~Configuration()
{
    if (hasMulticast) {
        delete multicastAddress;
    }
}

ReplicaAddress
Configuration::replica(int idx) const
{
    return replicas[idx];
}

ReplicaAddress
Configuration::replica(int group, int idx) const
{
    return g_replicas.at(group)[idx];
}

const ReplicaAddress *
Configuration::multicast() const
{
    if (hasMulticast) {
        return multicastAddress;
    } else {
        return nullptr;
    }
}

const ReplicaAddress *
Configuration::fc() const
{
    if (hasFC) {
        return fcAddress;
    } else {
        return nullptr;
    }
}

std::string
Configuration::Interface(int group, int idx) const
{
    return this->interfaces.at(group)[idx];
}

int
Configuration::GetLeaderIndex(view_t view) const
{
    return (view % this->n);
}

int
Configuration::QuorumSize() const
{
    return f+1;
}

int
Configuration::FastQuorumSize() const
{
    return f + (f+1)/2 + 1;
}

bool
Configuration::operator==(const Configuration &other) const
{
    if ((n != other.n) ||
        (f != other.f) ||
        (replicas != other.replicas) ||
        (hasMulticast != other.hasMulticast)) {
        return false;
    }

    if (hasMulticast) {
        if (*multicastAddress != *other.multicastAddress) {
            return false;
        }
    }

    return true;
}

bool
Configuration::operator<(const Configuration &other) const {
    auto this_t = std::forward_as_tuple(n, f, replicas, hasMulticast);
    auto other_t = std::forward_as_tuple(other.n, other.f, other.replicas,
                                         other.hasMulticast);
    if (this_t < other_t) {
        return true;
    } else if (this_t == other_t) {
        if (hasMulticast) {
            return *multicastAddress < *other.multicastAddress;
        } else {
            return false;
        }
    } else {
        // this_t > other_t
        return false;
    }
}

} // namespace transport
