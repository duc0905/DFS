#include <httplib.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "types.hpp"

#ifndef PORT
#define PORT 1234
#endif

using json = nlohmann::json;

int main() {
  std::cerr << "Hello from agent" << std::endl;

  std::string home_dir = getenv("HOME");
  std::filesystem::path datapath(home_dir + "/DFS/data");
  httplib::Server server;
  httplib::Client cmmu("localhost:4321");

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
    std::cerr << "Saving partition to " << std::filesystem::absolute(path) << std::endl;

    res.set_content("Received", "text/plain");
    res.status = httplib::StatusCode::Created_201;
  });

  /**
   * Called by CLI/user to write a file to our system
   */
  server.Post(
      "/write", [&cmmu](const httplib::Request& req, httplib::Response& res) {
        // TODO: Call CMMU/write
        //
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

  std::cerr << "Agent is listening at 0.0.0.0:" << PORT << std::endl;
  server.listen("0.0.0.0", PORT);

  return 0;
}
