#include <argparse/argparse.hpp>
#include <httplib.h>
#include <stduuid/uuid.h>

#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <nlohmann/json.hpp>

#include "types.hpp"

uint part_size;

// TODO: Use persistent storage
std::vector<FileMetadata> db;
std::vector<Agent> agents;

uint16_t add_agent(std::string address, uint16_t port) {
  uint16_t id;
  if (agents.size() == 0) {
    id = 1;
  } else {
    id = agents.back().m_id + 1;
  }

  agents.push_back({id, address, port});
  return id;
}

uint16_t find_agent(std::string address, uint16_t port) {
  for (auto& a : agents) {
    if (a.m_address == address && a.m_port == port) return a.m_id;
  }

  return 0;
}

/**
 * Get file metadata
 */
FileMetadata& get_file(const User& user, const std::string& filepath) {
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
  static uint16_t aidx = 1;
  FileMetadata::Partition part;

  aidx = (aidx + 1) % agents.size();
  Agent& a = agents[aidx];

  part.filepath = uuids::to_string(uuids::uuid_system_generator{}());
  part.part_id = part_id;
  part.agent_id = a.m_id;

  // Push data to that node
  httplib::MultipartFormDataItems items = {
      {"name", content, part.filepath, "application/octet-stream"}};
  auto res = a.m_conn.Post("/internal/write", items);

  if (res->status != 201) {
    throw std::runtime_error(std::format(
        "Failed to create partition %d on agent %d\n", part_id, a.m_id));
  }

  return part;
}

/**
 * Create a blank file
 */
FileMetadata& create_file(const User& user, const std::string& filepath) {
  try {
    get_file(user, filepath);
    throw FileExistsException(filepath);
  } catch (const FileDNEException& e) {
    // Create file
    FileMetadata newfile;
    newfile.filepath = filepath;
    if (db.size() > 0) {
      newfile.inode_number = db.back().inode_number + 1;
    } else {
      newfile.inode_number = 1;
    }

    newfile.size = 1;
    newfile.filetype = FileType::File;
    newfile.uid = user.uid;
    newfile.gid = 0;
    newfile.perm_flags = 0x7770;

    db.push_back(newfile);
    return db.back();
  }
}

FileMetadata& get_or_create_file(const User& user,
                                const std::string& filepath) noexcept {
  try {
    return get_file(user, filepath);
  } catch (const FileDNEException& e) {
    return create_file(user, filepath);
  }
}

FileMetadata write_file(const User& user, const std::string& filepath,
                        const std::string& content) {
  FileMetadata& metadata = get_or_create_file(user, filepath);
  auto n = content.size();

  metadata.size = n;
  metadata.partitions.clear();

  char buffer[part_size];
  uint64_t offset = 0;
  uint64_t count = 0;

  while (offset < n) {
    uint size = (part_size - 1 < n - offset) ? part_size - 1 : (n - offset);
    memset(buffer, 0, part_size);
    memcpy(buffer, &content[offset], size);
    auto part = create_partition(count++, std::string(buffer, size));
    offset += size;
    metadata.partitions.push_back(part);
  }

  // TODO: Save to db
  // We are using vector and reference

  return metadata;
}

int main(int argc, char* argv[]) {
  argparse::ArgumentParser program("CMMU");

  program.add_description("Centralized Metadata Management Unit for DFS.");

  program.add_argument("-h", "--host")
      .help("The host this CMMU listens to")
      .default_value<std::string>("0.0.0.0")
      .nargs(1);

  program.add_argument("-p", "--port")
      .help("The port this CMMU listens to")
      .default_value<uint>((uint)4321)
      .scan<'u', uint>()
      .nargs(1);

  program.add_argument("-P", "--part-size")
      .help("The part size in bytes for v1 write algorithm")
      .default_value((uint)(1024 * 1024)) // 1MB
      .scan<'u', uint>()
      .nargs(1);

  try {
    program.parse_args(argc, argv);
  } catch(const std::exception& e) {
    std::cerr << e.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  std::string host = program.get("-h");
  uint port = program.get<uint>("-p");
  part_size = program.get<uint>("-P");

  httplib::Server server;

  /**
   * Handles reading a file
   *
   * body: {
   *  filepath: string
   * }
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
      FileMetadata metadata = get_file({0}, body["filepath"]);

      // TODO: Check for permission

      json j_metadata = metadata;
      res.set_content(j_metadata.dump(), "application/json");
      res.status = httplib::StatusCode::OK_200;
    } catch (const FileDNEException& e) {
      std::cerr << "Error while getting file: " << e.what() << std::endl;
      res.status = httplib::StatusCode::NotFound_404;
      res.set_content(e.what(), "text/plain");
      return;
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

        // TODO: Check if file exist

        // name: the path for our fs
        // filename: original name of the file
        // content_type: content type
        // content: content
        auto& file = req.files.begin()->second;
        // Passing user with uid 0 for now
        // TODO: Use content receiver instead
        auto file_metadata = write_file({0}, file.name, file.content);
        try {
          json j_file_metadata = file_metadata;
          std::string s_file_metadata = j_file_metadata.dump();
          res.status = httplib::StatusCode::Created_201;
          res.set_content(s_file_metadata, "application/json");
        } catch (const std::exception& e) {
          std::cerr << "Error while jsonifying file metadata: " << e.what()
                    << std::endl;
        }
      });

  /**
   * Register agent
   *
   * Agents call this route to register to the cluster of this CMMU
   *
   * Body:
   *  - port: int
   */
  server.Post(
      "/register", [](const httplib::Request& req, httplib::Response& res) {
        json j_body;

        try {
          j_body = json::parse(req.body);
        } catch (const std::exception& e) {
          res.status = httplib::StatusCode::BadRequest_400;
          res.set_content(e.what(), "text/plain");
          return;
        }

        if (j_body.find("port") == j_body.end()) {
          res.status = httplib::StatusCode::BadRequest_400;
          res.set_content("Port is missing from the body", "text/plain");
          return;
        }

        uint16_t agent_port = j_body["port"];

        if (find_agent(req.remote_addr, agent_port) == 0) {
          add_agent(req.remote_addr, agent_port);
          res.status = httplib::StatusCode::Created_201;
          res.set_content("Registered", "text/plain");
        } else {
          res.status = httplib::StatusCode::Created_201;
          res.set_content("Welcome back", "text/plain");
        }
      });

  /**
   * Get all agents
   *
   * NOTE: Only be used for debugging
   * NOTE: Welp, now the agents are using this route to get other agents
   */
  server.Post("/agents",
              [](const httplib::Request& req, httplib::Response& res) {
                try {
                  json j_res = json::array();
                  for (auto& a : agents) {
                    json agent = json::object();
                    agent["id"] = a.m_id;
                    agent["address"] = a.m_address;
                    agent["port"] = a.m_port;
                    j_res.push_back(agent);
                  }

                  res.status = httplib::StatusCode::OK_200;
                  res.set_content(j_res.dump(), "application/json");

                } catch (const std::exception& e) {
                  std::cerr << "Error: " << e.what() << std::endl;
                  res.status = httplib::StatusCode::InternalServerError_500;
                  res.set_content(e.what(), "text/plain");
                }
              });

  // TODO: Add a default exception handler for server
  std::cerr << "Server is listening at " << host << ":" << port << std::endl;
  server.listen(host, port);

  return 0;
}
