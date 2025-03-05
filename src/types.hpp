#pragma once

#include <httplib.h>

#include <cstdint>
#include <cstring>
#include <exception>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

enum class FileType : uint8_t {
  File,
  Directory,
  SymLink,
  HardLink,
  Socket,
  None
};

struct FileMetadata {
  struct Partition {
    uint64_t part_id;      // ID of the partition, unique within a file
    uint16_t agent_id;     // ID of the node containing the partition
    std::string filepath;  // filepath on the node
  };

  std::string filepath;  // Absolute filepath of this DFS
  uint64_t inode_number;
  FileType filetype;
  uint64_t size;  // In bytes

  uint64_t uid;
  uint64_t gid;
  uint16_t perm_flags;  // oooogggguuuu____

  std::vector<Partition> partitions;
};

inline void to_json(json& j, const FileMetadata::Partition& p) {
  j = json{{"part_id", p.part_id},
           {"node_id", p.agent_id},
           {"filepath", p.filepath}};
}

inline void from_json(const json& j, FileMetadata::Partition& p) {
  j.at("part_id").get_to(p.part_id);
  j.at("node_id").get_to(p.agent_id);
  j.at("filepath").get_to(p.filepath);
}

inline void to_json(json& j, const FileMetadata& m) {
  j = json{{"filepath", m.filepath},
           {"inode_number", m.inode_number},
           {"filetype", m.filetype},
           {"size", m.size},
           {"uid", m.uid},
           {"gid", m.gid},
           {"perm_flags", m.perm_flags},
           {"partitions", m.partitions}};
}

inline void from_json(const json& j, FileMetadata& m) {
  j.at("filepath").get_to(m.filepath);
  j.at("inode_number").get_to(m.inode_number);
  j.at("filetype").get_to(m.filetype);
  j.at("size").get_to(m.size);
  j.at("uid").get_to(m.uid);
  j.at("gid").get_to(m.gid);
  j.at("perm_flags").get_to(m.perm_flags);
  // for (auto& j_p : j["partitions"]) {
  //   try {
  //     FileMetadata::Partition part = j_p;
  //     m.partitions.push_back(part);
  //     std::cerr << "Size: " << m.partitions.size() << std::endl;
  //   } catch (std::exception& e) {
  //     std::cerr << "FK ME: " << e.what() << std::endl;
  //   }
  // }

  j.at("partitions").get_to(m.partitions);
  std::cerr << "Size: " << m.partitions.size() << std::endl;
}

struct User {
  uint64_t uid;
};

class FileDNEException : public std::exception {
 public:
  // TODO: do something with the filepath
  FileDNEException(const std::string& filepath) : m_filepath(filepath) {}
  const char* what() const noexcept override { return "File does not exist"; }

  std::string m_filepath;
};

class FileExistsException : public std::exception {
 public:
  // TODO: do something with the filepath
  FileExistsException(const std::string& filepath) : m_filepath(filepath) {}
  const char* what() const noexcept override { return "File exists"; }

  std::string m_filepath;
};

class Agent {
 public:
  Agent(uint16_t id, std::string address, uint16_t port)
      : m_id(id),
        m_address(address),
        m_port(port),
        m_conn(httplib::Client(address, port)) {}

 public:
  uint16_t m_id;
  std::string m_address;
  uint16_t m_port;
  httplib::Client m_conn;
};
