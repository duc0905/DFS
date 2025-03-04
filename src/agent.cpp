#include <httplib.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "types.hpp"

#ifndef PORT
#define PORT 1234
#endif

using json = nlohmann::json;

int main() {
  std::cerr << "Hello from agent" << std::endl;

  // TODO: Use arguments for addresses and ports
  std::string home_dir = getenv("HOME");
  std::filesystem::path datapath(home_dir + "/DFS/data");

  httplib::Client cmmu("localhost:4321");
  httplib::Server server;

  /**
   * NOTE: Should only be called by CMMU
   *
   * Store a partition on the current node
   */
  server.Post("/internal/write", [&datapath](const httplib::Request& req,
                                             httplib::Response& res) {
    if (req.files.size() != 1) {
      res.set_content("This route takes exactly 1 file", "text/plain");
      res.status = httplib::StatusCode::BadRequest_400;
      return;
    }

    auto file = req.files.begin()->second;
    auto path = datapath / file.filename;

    try {
      std::ofstream f(path, std::ios::binary);
      if (!f) {
        std::cerr << "File open failed" << std::endl;

        res.set_content("Failed", "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
        return;
      }

      std::cerr << "Saving partition to " << path << std::endl;
      std::cerr << "Content: " << file.content << std::endl;

      f << file.content;
    } catch (const std::exception& e) {
      std::cerr << "Error while writing content to file: " << e.what()
                << std::endl;
      res.set_content(e.what(), "text/plain");
      res.status = httplib::StatusCode::InternalServerError_500;
      return;
    }

    res.set_content("Received", "text/plain");
    res.status = httplib::StatusCode::Created_201;
  });

  /**
   * Called by CLI/user to write a file to our system
   */
  server.Post(
      "/write", [&cmmu](const httplib::Request& req, httplib::Response& res) {
        // name, content, filename, content-type
        auto size = req.files.size();

        if (size != 1) {
          res.status = httplib::StatusCode::BadRequest_400;
          res.set_content("This API only allow writing to exactly 1 file",
                          "text/plain");
          return;
        }

        std::cerr << "Forwarding file to CMMU" << std::endl;

        httplib::MultipartFormDataItems item = {req.files.begin()->second};

        auto result = cmmu.Post("/write", item);
        if (result) {
          // result->status;
          std::cerr << result->body;
          res.set_content(result->body, "text/plain");
          res.status = httplib::StatusCode::Created_201;
        } else {
          std::cerr << "Error while sending to CMMU: " << result.error()
                    << std::endl;
          res.set_content(httplib::to_string(result.error()), "text/plain");
          res.status = httplib::StatusCode::InternalServerError_500;
        }
      });

  server.Post("/internal/read", [&datapath](const httplib::Request& req,
                                            httplib::Response& res) {
    json j_body;
    std::string filepath;
    try {
      j_body = json::parse(req.body);
    } catch (const std::exception& e) {
      std::cerr << "Error while parsing JSON: " << e.what() << std::endl;
      res.set_content(e.what(), "text/plain");
      res.status = httplib::StatusCode::BadRequest_400;
      return;
    }

    // Validating request
    if (!j_body.contains("filepath")) {
      res.set_content("Request does not contain filepath", "text/plain");
      res.status = httplib::StatusCode::BadRequest_400;
      return;
    }

    filepath = j_body["filepath"];
    auto path = datapath / filepath;

    try {
      std::ifstream f(path, std::ios::binary);
      if (!f) {
        res.set_content("File/partition does not exist", "text/plain");
        res.status = httplib::StatusCode::NotFound_404;
      } else {
        std::string content;
        f >> content;
        res.status = httplib::StatusCode::OK_200;
        res.set_content(content, "application/octet-stream");
      }
    } catch (const std::exception& e) {
      std::cerr << "Error while reading file: " << e.what() << std::endl;
      res.set_content(e.what(), "text/plain");
      res.status = httplib::StatusCode::InternalServerError_500;
    }
  });

  server.Post("/read", [&datapath, &cmmu](const httplib::Request& req,
                                          httplib::Response& res) {
    json j_body;
    std::string filepath;
    try {
      j_body = json::parse(req.body);
    } catch (const std::exception& e) {
      std::cerr << "Error while parsing JSON: " << e.what() << std::endl;
      res.set_content(e.what(), "text/plain");
      res.status = httplib::StatusCode::BadRequest_400;
      return;
    }

    // Validating request
    if (!j_body.contains("filepath")) {
      res.set_content("Request does not contain filepath", "text/plain");
      res.status = httplib::StatusCode::BadRequest_400;
      return;
    }

    filepath = j_body["filepath"];

    // Get file metadata
    {
      json j_body = json::object();
      j_body["filepath"] = filepath;
      auto result = cmmu.Post("/", j_body.dump(), "application/json");

      if (!result) {
        res.set_content("Failed to get metadata", "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
        return;
      }

      if (result->status != 200) {
        res.set_content(result->body, "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
        return;
      }

      json j_metadata;
      try {
        j_metadata = json::parse(result->body);
      } catch (const std::exception& e) {
        std::cerr << "Error while parsing JSON: " << e.what() << std::endl;
        res.set_content(e.what(), "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
        return;
      }

      FileMetadata metadata;
      try {
        metadata = j_metadata;
      } catch (const std::exception& e) {
        std::cerr << "Error while casting json to FileMetadata: " << e.what()
                  << std::endl;
        res.set_content(e.what(), "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
        return;
      }

      // TODO: Call to agents to get partitions
      metadata.partitions[0].agent_id;
    }
  });

  /**
   * Called by CLI/user to read a file in our system
   *
   * body: {
   *   filepath: string
   * }
   */
  server.Post("/read", [&cmmu](const auto& req, auto& res) {
    json j_body;
    std::string filepath;
    try {
      j_body = json::parse(req.body);
    } catch (const std::exception& e) {
      std::cerr << "Error while parsing JSON: " << e.what() << std::endl;
      res.set_content(e.what(), "text/plain");
      res.status = httplib::StatusCode::BadRequest_400;
      return;
    }

    // Validating request
    if (!j_body.contains("filepath")) {
      res.set_content("Request does not contain filepath", "text/plain");
      res.status = httplib::StatusCode::BadRequest_400;
      return;
    }

    filepath = j_body["filepath"];
    json j_req_body = {{"filepath", filepath}};

    // Getting filemetadata
    // TODO: Pass body
    auto result = cmmu.Post("/stat");
    if (!result) {  // Error
      std::cerr << "Error while sending to CMMU: " << result.error()
                << std::endl;
      res.set_content(httplib::to_string(result.error()), "text/plain");
      res.status = httplib::StatusCode::InternalServerError_500;
      return;
    }

    // Validating request
    json j_metadata;
    try {
      j_metadata = json::parse(result->body);
    } catch (const std::exception& e) {
      std::cerr << "Error while parsing JSON: " << e.what() << std::endl;
      res.set_content(e.what(), "text/plain");
      res.status = httplib::StatusCode::BadRequest_400;
      return;
    }
  });

  // TODO: Add a default exception handler for server

  // NOTE: Call register API on CMMU

  {
    json j_body = json::object();
    j_body["port"] = PORT;
    auto result = cmmu.Post("/register", j_body.dump(), "application/json");
    if (result->status != 200 && result->status != 201) {
      std::cerr << "Failed to register to CMMU: " << result.error()
                << std::endl;
      std::cerr << "Status: " << result->status << std::endl;
      return 1;
    } else {
      std::cerr << "Successfully registered to CMMU" << std::endl;
      std::cerr << "Body: " << result->body << std::endl;
    }
  }

  std::cerr << "Agent is listening at 0.0.0.0:" << PORT << std::endl;
  server.listen("0.0.0.0", PORT);

  return 0;
}
