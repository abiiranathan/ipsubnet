#include <arpa/inet.h> // for inet_addr
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <emscripten/bind.h>
#include <iostream>
#include <math.h>
#include <netinet/in.h> // for ntohl
#include <ostream>
#include <stddef.h>
#include <stdio.h>
#include <vector>

using namespace emscripten;

struct Subnet {
  uint32_t ip;                // 32-bit IP Address
  uint32_t mask;              // 32-bit subnet mask
  unsigned int prefix_length; // Subnet mask prefix length
};

struct IPAddress {
  uint32_t ip;   // 32-bit IP Address
  char ip_class; // Class for this IP Address
};

struct SubnetInfo {
  std::string network_id;       // The network id in CIDR notation.
  std::string host_range_start; // Starting ip in subnet range.
  std::string host_range_end;   // Last IP in subnet range.
  std::string broadcast_id;     // Broadcast ID
  std::string subnet_mask;      // New Subnet mask for this subnet.
  uint16_t num_usable_hosts;    // Number of usable hosts for this subnet
};

// The subnetting table
// Forexample: 1 subnet guarantees 256 total_hosts
// with a CIDR /24
const uint16_t SUBNET_TABLE[3][9] = {
    {1, 2, 4, 8, 16, 32, 64, 128, 256},    // num_subnets
    {256, 128, 64, 32, 16, 8, 4, 2, 1},    // num_host_ids
    {24, 25, 26, 27, 28, 29, 30, 31, 32}}; // CIDR

class SubnetCalculator {
private:
  static const uint32_t ALL_ONES_MASK = 0xFFFFFFFFU;

public:
  SubnetCalculator();

  Subnet create_subnet(const std::string &subnet_string) {
    Subnet subnet;

    char ip_part[16];
    unsigned int prefix_length;
    if (sscanf(subnet_string.c_str(), "%15[^/]/%d", ip_part, &prefix_length) !=
        2) {
      perror("Invalid subnet string format");
      exit(EXIT_FAILURE);
    }

    uint32_t ip = inet_addr(ip_part);
    if (ip == INADDR_NONE) {
      perror("Invalid IP address format");
      exit(EXIT_FAILURE);
    }

    if (prefix_length < 24 || prefix_length > 32) {
      perror("Unsupported CIDR prefix. Supported range is from /24 to /32");
      exit(EXIT_FAILURE);
    }

    subnet.ip = ntohl(ip);
    subnet.mask = ALL_ONES_MASK << (32 - static_cast<uint32_t>(prefix_length));
    subnet.prefix_length = prefix_length;
    return subnet;
  }

  char classify_ip(uint32_t addr) {
    if ((addr & 0x80000000U) == 0) {
      return 'A'; // Class A
    } else if ((addr & 0xC0000000U) == 0x80000000U) {
      return 'B'; // Class B
    } else if ((addr & 0xE0000000U) == 0xC0000000U) {
      return 'C'; // Class C
    } else if ((addr & 0xF0000000U) == 0xE0000000U) {
      return 'D'; // Class D (Multicast)
    } else {
      return 'E'; // Class E (Reserved)
    }
  }

  void assignable_range(Subnet subnet, IPAddress *start, IPAddress *end) {
    start->ip = (subnet.ip & subnet.mask) + 1;
    start->ip_class = classify_ip(start->ip);

    end->ip = (subnet.ip | ~subnet.mask) - 1;
    end->ip_class = classify_ip(end->ip);
  }

  unsigned int compute_assignable_addresses(std::string network_id) {
    char *token = std::strtok((char *)network_id.data(), "/");
    std::string ip_address = token;

    token = std::strtok(nullptr, "/");
    int prefix_length = std::atoi(token);

    // Check if the prefix length is valid.
    if (prefix_length < 0 || prefix_length > 31) {
      return 0;
    }

    // Calculate the number of assignable host IDs.
    int number_of_host_ids = (int)pow(2, 32 - prefix_length) - 2;
    return number_of_host_ids;
  }

  std::string to_ipv4_string(uint32_t ip) {
    char addr[16];
    snprintf(addr, 16, "%u.%u.%u.%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF, ip & 0xFF);

    return std::string(addr);
  }

  void print_ips_in_subnet(Subnet subnet) {
    for (uint32_t ip = subnet.ip; ip <= (subnet.ip | ~subnet.mask); ip++) {
      printf("%u.%u.%u.%u\n", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF, ip & 0xFF);
    }
  }

  std::vector<SubnetInfo> get_subnet_table(const std::string &networkID,
                                           uint16_t num_subnets) {

    if (num_subnets < 1 || num_subnets > 256) {
      printf("num_subnets must be between 1 and 256\n");
      return std::vector<SubnetInfo>();
    }

    std::vector<SubnetInfo> subnet_info;
    subnet_info.reserve(num_subnets);

    uint16_t nearest_subnet = 1;
    uint16_t nearest_index = 0;
    while (nearest_subnet < num_subnets) {
      nearest_subnet <<= 1;
      nearest_index++;
    }

    Subnet subnet = create_subnet(networkID);
    uint16_t total_num_hosts = SUBNET_TABLE[1][nearest_index];
    uint16_t new_subnet_mask = SUBNET_TABLE[2][nearest_index];
    uint32_t networkID_int = subnet.ip;

    for (uint16_t i = 0; i < num_subnets; i++) {
      uint32_t start_range = networkID_int + 1;
      uint32_t end_range = networkID_int + total_num_hosts - 2;

      char network_id[16], subnet_mask[24], host_range_start[16],
          host_range_end[16], broadcast_id[16];

      snprintf(network_id, sizeof(network_id), "%u.%u.%u.%u",
               (networkID_int >> 24) & 0xFF, (networkID_int >> 16) & 0xFF,
               (networkID_int >> 8) & 0xFF, networkID_int & 0xFF);

      snprintf(subnet_mask, sizeof(subnet_mask), "/%d", new_subnet_mask);

      snprintf(host_range_start, sizeof(host_range_start), "%u.%u.%u.%u",
               (start_range >> 24) & 0xFF, (start_range >> 16) & 0xFF,
               (start_range >> 8) & 0xFF, start_range & 0xFF);

      snprintf(host_range_end, sizeof(host_range_end), "%u.%u.%u.%u",
               (end_range >> 24) & 0xFF, (end_range >> 16) & 0xFF,
               (end_range >> 8) & 0xFF, end_range & 0xFF);

      snprintf(broadcast_id, sizeof(broadcast_id), "%u.%u.%u.%u",
               (networkID_int + total_num_hosts - 1) >> 24 & 0xFF,
               (networkID_int + total_num_hosts - 1) >> 16 & 0xFF,
               (networkID_int + total_num_hosts - 1) >> 8 & 0xFF,
               (networkID_int + total_num_hosts - 1) & 0xFF);

      SubnetInfo info;
      info.num_usable_hosts = total_num_hosts - 2;
      info.network_id = network_id;
      info.subnet_mask = subnet_mask;
      info.host_range_start = host_range_start;
      info.host_range_end = host_range_end;
      info.broadcast_id = broadcast_id;

      subnet_info.emplace_back(info);
      networkID_int += (uint32_t)total_num_hosts;
    }
    return subnet_info;
  }

  void print_subnet_table(const std::vector<SubnetInfo> &subnet_info) {
    printf("%s\n",
           "-------------------------------------------------------------"
           "-----------------------------------------------------");
    printf("%-18s | %-25s | %-33s | %-10s | %-18s\n", "Network ID",
           "Subnet Mask", "Host ID Range", "# Usable", "Broadcast ID");

    printf("%s\n",
           "-------------------------------------------------------------"
           "-----------------------------------------------------");

    for (const SubnetInfo &info : subnet_info) {
      printf("%-18s | %-25s | %-15s - %-15s | %-10u | %-18s\n",
             info.network_id.c_str(), info.subnet_mask.c_str(),
             info.host_range_start.c_str(), info.host_range_end.c_str(),
             info.num_usable_hosts, info.broadcast_id.c_str());
    }
  }
};

SubnetCalculator::SubnetCalculator(){};

// Create emcripten bindings
EMSCRIPTEN_BINDINGS(subnet_calculator) {
  value_object<Subnet>("Subnet")
      .field("ip", &Subnet::ip)
      .field("mask", &Subnet::mask);

  value_object<IPAddress>("IPAddress")
      .field("ip", &IPAddress::ip)
      .field("ip_class", &IPAddress::ip_class);

  value_object<SubnetInfo>("SubnetInfo")
      .field("network_id", &SubnetInfo::network_id)
      .field("host_range_start", &SubnetInfo::host_range_start)
      .field("host_range_end", &SubnetInfo::host_range_end)
      .field("broadcast_id", &SubnetInfo::broadcast_id)
      .field("subnet_mask", &SubnetInfo::subnet_mask)
      .field("num_usable_hosts", &SubnetInfo::num_usable_hosts);

  class_<SubnetCalculator>("SubnetCalculator")
      .constructor()
      .function("create_subnet", &SubnetCalculator::create_subnet)
      .function("classify_ip", &SubnetCalculator::classify_ip)
      .function("compute_assignable_addresses",
                &SubnetCalculator::compute_assignable_addresses)
      .function("to_ipv4_string", &SubnetCalculator::to_ipv4_string)
      .function("get_subnet_table", &SubnetCalculator::get_subnet_table)
      .function("print_subnet_table", &SubnetCalculator::print_subnet_table);

  register_vector<std::string>("StringVector");
  register_vector<SubnetInfo>("SubnetInfoVector");
}
