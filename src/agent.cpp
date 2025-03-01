#include <httplib.h>

#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>

#ifndef PORT
#define PORT 1234
#endif

using json = nlohmann::json;

int main() {
  std::cerr << "Hello from agent" << std::endl;

  httplib::Server server;
  httplib::Client cmmu("localhost:1234");

  /**
   * NOTE: Should only be called by CMMU
   *
   */
  server.Post("/internal/write", [](const httplib::Request& req,
                                    httplib::Response& res) {
    for (auto& file : req.files) {
      std::cerr << "Name: " << file.second.name << std::endl
                << "Filename: " << file.second.filename << std::endl
                << "Content type: " << file.second.content_type << std::endl
                << "Content: " << file.second.content << std::endl;
    }

    res.set_content("Received", "text/plain");
    res.status = httplib::StatusCode::Created_201;
  });

  /**
   * Called by CLI/user to write a file to our system
   */
  server.Post("/write", [&cmmu](const httplib::Request& req,
                                httplib::Response& res) {
    // Call CMMU/write
    // name, content, filename, content-type
    httplib::MultipartFormDataItems item = {{"name", "content", "filename", "content type"}};

    auto result = cmmu.Post("/write", item);
    if (result) {
      // result->status;
      std::cerr << result->body;
      res.set_content(result->body, "text/plain");
      res.status = httplib::StatusCode::Created_201;
    } else {
      std::cerr << "Error while sending to CMMU: " << result.error() << std::endl;
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
    } catch(const std::exception& e) {
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
    auto result = cmmu.Post("/stat", j_req_body);
    if (!result) { // Error
      std::cerr << "Error while sending to CMMU: " << result.error() << std::endl;
      res.set_content(httplib::to_string(result.error()), "text/plain");
      res.status = httplib::StatusCode::InternalServerError_500;
      return;
    }

    // Validating request
    json j_metadata;
    try {
      j_metadata = json::parse(result->body);
    } catch(const std::exception& e) {
      std::cerr << "Error while parsing JSON: " << e.what() << std::endl;
      res.set_content(e.what(), "text/plain");
      res.status = httplib::StatusCode::BadRequest_400;
      return;
    }


    // Getting metadata
    std::string filepath = j_metadata["filepath"];
  });

  std::cerr << "Agent is listening at 0.0.0.0:" << PORT << std::endl;
  server.listen("0.0.0.0", PORT);

  return 0;
}
