// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/iptables.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <chromeos/process.h>

namespace {
const char kIptablesPath[] = "/sbin/iptables";
}

namespace firewalld {

IpTables::IpTables() : IpTables{kIptablesPath} {}

IpTables::IpTables(const std::string& path) : executable_path_{path} {}

bool IpTables::PunchTcpHole(chromeos::ErrorPtr* error,
                            uint16_t in_port,
                            bool* out_success) {
  *out_success = PunchHole(in_port, &tcp_holes_, kProtocolTcp);
  return true;
}

bool IpTables::PunchUdpHole(chromeos::ErrorPtr* error,
                            uint16_t in_port,
                            bool* out_success) {
  *out_success = PunchHole(in_port, &udp_holes_, kProtocolUdp);
  return true;
}

bool IpTables::PlugTcpHole(chromeos::ErrorPtr* error,
                           uint16_t in_port,
                           bool* out_success) {
  *out_success = PlugHole(in_port, &tcp_holes_, kProtocolTcp);
  return true;
}

bool IpTables::PlugUdpHole(chromeos::ErrorPtr* error,
                           uint16_t in_port,
                           bool* out_success) {
  *out_success = PlugHole(in_port, &udp_holes_, kProtocolUdp);
  return true;
}

bool IpTables::PunchHole(uint16_t port,
                         std::unordered_set<uint16_t>* holes,
                         enum ProtocolEnum protocol) {
  if (port == 0) {
    // Port 0 is not a valid TCP/UDP port.
    return false;
  }

  if (holes->find(port) != holes->end()) {
    // We have already punched a hole for |port|.
    // Be idempotent: do nothing and succeed.
    return true;
  }

  std::string sprotocol = protocol == kProtocolTcp ? "TCP" : "UDP";
  LOG(INFO) << "Punching hole for " << sprotocol << " port " << port;
  if (!IpTables::AddAllowRule(executable_path_, protocol, port)) {
    // If the 'iptables' command fails, this method fails.
    LOG(ERROR) << "Calling 'iptables' failed";
    return false;
  }

  // Track the hole we just punched.
  holes->insert(port);

  return true;
}

bool IpTables::PlugHole(uint16_t port,
                        std::unordered_set<uint16_t>* holes,
                        enum ProtocolEnum protocol) {
  if (port == 0) {
    // Port 0 is not a valid TCP/UDP port.
    return false;
  }

  if (holes->find(port) == holes->end()) {
    // There is no firewall hole for |port|.
    // Even though this makes |PlugHole| not idempotent,
    // and Punch/Plug not entirely symmetrical, fail. It might help catch bugs.
    return false;
  }

  std::string sprotocol = protocol == kProtocolTcp ? "TCP" : "UDP";
  LOG(INFO) << "Plugging hole for " << sprotocol << " port " << port;
  if (!IpTables::DeleteAllowRule(executable_path_, protocol, port)) {
    // If the 'iptables' command fails, this method fails.
    LOG(ERROR) << "Calling 'iptables' failed";
    return false;
  }

  // Stop tracking the hole we just plugged.
  holes->erase(port);

  return true;
}

// static
bool IpTables::AddAllowRule(const std::string& path,
                            enum ProtocolEnum protocol,
                            uint16_t port) {
  chromeos::ProcessImpl iptables;
  iptables.AddArg(path);
  iptables.AddArg("-A");  // append
  iptables.AddArg("INPUT");
  iptables.AddArg("-p");  // protocol
  iptables.AddArg(protocol == kProtocolTcp ? "tcp" : "udp");
  iptables.AddArg("--dport");  // destination port
  std::string port_number = base::StringPrintf("%d", port);
  iptables.AddArg(port_number.c_str());
  iptables.AddArg("-j");
  iptables.AddArg("ACCEPT");

  return iptables.Run() == 0;
}

// static
bool IpTables::DeleteAllowRule(const std::string& path,
                               enum ProtocolEnum protocol,
                               uint16_t port) {
  chromeos::ProcessImpl iptables;
  iptables.AddArg(path);
  iptables.AddArg("-D");  // delete
  iptables.AddArg("INPUT");
  iptables.AddArg("-p");  // protocol
  iptables.AddArg(protocol == kProtocolTcp ? "tcp" : "udp");
  iptables.AddArg("--dport");  // destination port
  std::string port_number = base::StringPrintf("%d", port);
  iptables.AddArg(port_number.c_str());
  iptables.AddArg("-j");
  iptables.AddArg("ACCEPT");

  return iptables.Run() == 0;
}

}  // namespace firewalld