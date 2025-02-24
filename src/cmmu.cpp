#include <httplib.h>
#include <stduuid/uuid.h>

#include <cstring>
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>

#ifndef PORT
#define PORT 4321
#endif  // !PORT

#define PART_SIZE 4

using json = nlohmann::json;

// TODO: Seperate type defs to a header file to reuse in Agent


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
    uint64_t node_id;      // ID of the node containing the partition
    std::string filepath;  // Relative filepath on the node
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

void to_json(json& j, const FileMetadata::Partition& p) {
  j = json{
      {"part_id", p.part_id}, {"node_id", p.node_id}, {"filepath", p.filepath}};
}

void from_json(const json& j, FileMetadata::Partition& p) {
  j.at("part_id").get_to(p.part_id);
  j.at("node_id").get_to(p.node_id);
  j.at("filepath").get_to(p.filepath);
}

void to_json(json& j, const FileMetadata& m) {
  j = json{{"filepath", m.filepath},
           {"inode_number", m.inode_number},
           {"filetype", m.filetype},
           {"size", m.size},
           {"uid", m.uid},
           {"gid", m.gid},
           {"perm_flags", m.perm_flags},
           {"partitions", m.partitions}
  };
}

void from_json(const json& j, FileMetadata& m) {
  j.at("filepath").get_to(m.filepath);
  j.at("inode_number").get_to(m.inode_number);
  j.at("filetype").get_to(m.filetype);
  j.at("size").get_to(m.size);
  j.at("uid").get_to(m.uid);
  j.at("gid").get_to(m.gid);
  j.at("perm_flags").get_to(m.perm_flags);
  j.at("partitions").get_to(m.partitions);
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

std::vector<FileMetadata> db;

/**
 * Get file metadata
 */
FileMetadata get_file(const User& user, const std::string& filepath) {
  for (auto& file : db) {
    if (file.filepath == filepath) return file;
  }

  throw FileDNEException(filepath);
}

/**
 * Create a file partition
 */
FileMetadata::Partition create_partition(const uint64_t& part_id,
                                         const std::string& content) {
  FileMetadata::Partition part;
  part.filepath = uuids::to_string(uuids::uuid_system_generator{}());
  part.part_id = part_id;
  part.node_id = 0;  // TODO: randomly choose a node

  // TODO: Push data to that node

  return part;
}

/**
 * Create a blank file
 */
FileMetadata create_file(const User& user, const std::string& filepath) {
  try {
    get_file(user, filepath);
    throw FileExistsException(filepath);
  } catch (const FileDNEException& e) {
    // Create file
    FileMetadata newfile;
    newfile.filepath = filepath;
    if (db.size() > 0) {
      newfile.inode_number = db.end()->inode_number + 1;
    } else {
      newfile.inode_number = 1;
    }

    newfile.size = 1;
    newfile.filetype = FileType::File;
    newfile.uid = user.uid;
    newfile.gid = 0;
    newfile.perm_flags = 0x7770;
    newfile.partitions.push_back(create_partition(0, ""));
    return newfile;
  }
}

FileMetadata get_or_create_file(const User& user,
                                const std::string& filepath) noexcept {
  try {
    return get_file(user, filepath);
  } catch (const FileDNEException& e) {
    return create_file(user, filepath);
  }
}

FileMetadata write_file(const User& user, const std::string& filepath,
                        const std::string& content) {
  FileMetadata metadata = get_or_create_file(user, filepath);
  auto n = content.size();

  metadata.size = n;
  metadata.partitions.clear();

  char buffer[PART_SIZE];
  uint64_t offset = 0;
  uint64_t count = 0;
  while (offset < n) {
    uint size = (PART_SIZE < n - offset)
                    ? PART_SIZE
                    : (n - offset);
    memset(buffer, 0, PART_SIZE);
    memcpy(buffer, &content[offset], size);
    auto part = create_partition(count++, buffer);
    offset += size;
    metadata.partitions.push_back(part);
  }

  // TODO: Save to real DB
  db.push_back(metadata);

  return metadata;
}

int main() {
  httplib::Server server;

  /**
   * Handles reading a file
   */
  server.Post("/stat", [](const httplib::Request& req, httplib::Response& res) {
    // Parse the body of the request
    json body;
    try {
      body = json::parse(req.body);
    } catch (const json::parse_error&) {
      res.status = httplib::StatusCode::BadRequest_400;
      res.set_content("Invalid body", "text/plain");
      return;
    }

    try {
      auto metadata = get_file({0}, body["filepath"]);

      // TODO: Check for permission

      json j_metadata = metadata;
      res.set_content(j_metadata, "application/json");
      res.status = httplib::StatusCode::OK_200;
    } catch (const std::exception& e) {  // TODO: Handle different exceptions
      std::cerr << "Error while getting file: " << e.what() << std::endl;
      res.status = httplib::StatusCode::BadRequest_400;
      res.set_content(e.what(), "text/plain");
      return;
    }
  });

  /**
   * Handles writing to a single file
   */
  server.Post(
      "/write", [](const httplib::Request& req, httplib::Response& res) {
        auto size = req.files.size();

        if (size != 1) {
          res.status = httplib::StatusCode::BadRequest_400;
          res.set_content("This API only allow writing to exactly 1 file",
                          "text/plain");
          return;
        }

        // name: the path for our fs
        // filename: original name of the file
        // content_type: content type
        // content: content
        auto& file = req.files.begin()->second;
        // Passing user with uid 0 for now
        auto file_metadata = write_file({0}, file.name, file.content);
        try {
          json j_file_metadata = file_metadata;
          std::string s_file_metadata = j_file_metadata.dump();
          res.status = httplib::StatusCode::Created_201;
          res.set_content(s_file_metadata, "application/json");
        } catch (const std::exception& e) {
          std::cerr << "Error while jsonifying file metadata: " << e.what() << std::endl;
        }
      });

  std::cerr << "Server is listening at 0.0.0.0:" << PORT << std::endl;
  server.listen("0.0.0.0", PORT);

  return 0;
}
