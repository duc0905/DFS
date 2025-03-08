#include <httplib.h>

#include <argparse/argparse.hpp>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "types.hpp"

using json = nlohmann::json;

std::vector<Agent> agents;

std::vector<Agent> get_agents(httplib::Client& cmmu) {
  auto result = cmmu.Post("/agents");

  if (!result) {
    throw new std::runtime_error(
        std::string("Error while getting agents from CMMU: %s\n") +
        httplib::to_string(result.error()));
  } else {
    json j_body;
    std::vector<Agent> ret;
    try {
      j_body = json::parse(result->body);
    } catch (const std::exception& e) {
      throw new std::runtime_error(std::string("Invalid response body: ") +
                                   e.what() + "\n");
    }

    for (auto& j_agent : j_body) {
      ret.push_back({j_agent["id"], j_agent["address"], j_agent["port"]});
    }

    return ret;
  }
}

int main(int argc, char* argv[]) {
  argparse::ArgumentParser program("Agent");

  program.add_argument("-h", "--host")
      .help("The host this agent listens to")
      .default_value<std::string>("0.0.0.0")
      .nargs(1);

  program.add_argument("-p", "--port")
      .help("The port this agent listens to")
      .default_value<uint>(1234)
      .scan<'u', uint>()
      .nargs(1);

  program.add_argument("cmmu-host")
      .help("The address of the CMMU")
      .required()
      .nargs(1);

  program.add_argument("cmmu-port")
      .help("The port the CMMU is listening on")
      .required()
      .scan<'u', uint>()
      .nargs(1);

  program.add_argument("-d", "--directory")
      .help("The local directory to store the files")
      .default_value("/tmp/dfs")
      .nargs(1);

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  std::string cmmu_host, host;
  uint cmmu_port, port;

  try {  // Parsing CMMU address
    cmmu_host = program.get("cmmu-host");
    cmmu_port = program.get<uint>("cmmu-port");
  } catch (const std::exception& e) {
    std::cerr << "Error while parsing CMMU address: " << e.what() << std::endl;
    return 1;
  }

  httplib::Client cmmu(cmmu_host, cmmu_port);

  try {  // Parsing agent address
    host = program.get("-h");
    port = program.get<uint>("-p");
  } catch (const std::exception& e) {
    std::cerr << "Error while parsing Agent address: " << e.what() << std::endl;
    return 1;
  }

  std::filesystem::path datapath(program.get("-d"));

  // TODO: Check if the datapath exist and valid

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

      f.write(file.content.data(), file.content.size());
      f.close();
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
        res.set_chunked_content_provider(
            "application/octet-stream",
            [path](size_t offset, httplib::DataSink& sink) {  // Read file
              std::ifstream f(path, std::ios::binary);
              if (!f) {
                sink.done();
                return false;
              }

              char buffer[1024];
              memset(buffer, 0, sizeof buffer);
              while (!f.eof()) {
                f.read(buffer, sizeof buffer);
                sink.write(buffer, f.gcount());
              }

              f.close();

              sink.done();
              return true;
            });
      }
    } catch (const std::exception& e) {
      std::cerr << "Error while reading file: " << e.what() << std::endl;
      res.set_content(e.what(), "text/plain");
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
      auto result = cmmu.Post("/stat", j_body.dump(), "application/json");

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
        std::cerr << "Size2: " << metadata.partitions.size() << std::endl;
      } catch (const std::exception& e) {
        std::cerr << "Error while casting json to FileMetadata: " << e.what()
                  << std::endl;
        res.set_content(e.what(), "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
        return;
      }

      // Getting all the agents from the CMMU
      try {
        agents = get_agents(cmmu);
      } catch (const std::exception& e) {
        std::cerr << "Error while getting agent: " << e.what() << std::endl;
        res.set_content(e.what(), "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
        return;
      }

      // TODO: Call to agents to get partitions
      res.set_chunked_content_provider(
          "application/octet-stream",
          [metadata](size_t offset, httplib::DataSink& sink) {
            std::cerr << "Fk3 " << metadata.partitions.size() << std::endl;
            for (auto& part : metadata.partitions) {
              json j_body = json::object();
              j_body["filepath"] = part.filepath;
              std::cerr << "Getting data for part " << part.part_id
                        << std::endl;
              for (auto& agent : agents) {
                if (agent.m_id == part.agent_id) {
                  std::cerr << "Sending req to " << agent.m_address
                            << std::endl;
                  auto result = agent.m_conn.Post(
                      "/internal/read", j_body.dump(), "application/json");
                  if (!result) {
                    std::cerr << "Failed" << std::endl;
                    return false;
                  }

                  std::cerr << "Part " << part.part_id << ":" << std::endl;
                  std::cerr << result->body << std::endl;
                  sink.write(result->body.data(), result->body.size());
                }
              }
            }

            sink.done();
            return true;
          });
    }
  });

  /**
   * Called by CLI/user to read a file in our system
   *
   * body: {
   *   filepath: string
   * }
   */
  server.Get("/read", [&datapath, &cmmu](const httplib::Request& req,
                                         httplib::Response& res) {
    std::string filepath;
    FileMetadata metadata;

    if (!req.has_param("filepath")) {
      res.set_content("Request does not contain filepath", "text/plain");
      res.status = httplib::StatusCode::BadRequest_400;
      return;
    }

    filepath = req.get_param_value("filepath");

    {  // NOTE: Get file metadata
      json j_body = json::object();
      json j_metadata;
      j_body["filepath"] = filepath;
      auto result = cmmu.Post("/stat", j_body.dump(), "application/json");

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

      try {
        j_metadata = json::parse(result->body);
      } catch (const std::exception& e) {
        std::cerr << "Error while parsing JSON: " << e.what() << std::endl;
        res.set_content(e.what(), "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
        return;
      }

      try {
        metadata = j_metadata;
      } catch (const std::exception& e) {
        std::cerr << "Error while casting json to FileMetadata: " << e.what()
                  << std::endl;
        res.set_content(e.what(), "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
        return;
      }
    }

    // Getting all the agents from the CMMU
    {
      try {
        agents = get_agents(cmmu);
      } catch (const std::exception& e) {
        std::cerr << "Error while getting agent: " << e.what() << std::endl;
        res.set_content(e.what(), "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
        return;
      }
    }

    res.set_chunked_content_provider(
        "application/octet-stream",
        [metadata](size_t offset, httplib::DataSink& sink) {
          for (auto& part : metadata.partitions) {
            json j_body = json::object();
            j_body["filepath"] = part.filepath;
            for (auto& agent : agents) {
              // NOTE: Call to agents to get partitions
              if (agent.m_id == part.agent_id) {
                auto result = agent.m_conn.Post("/internal/read", j_body.dump(),
                                                "application/json");
                if (!result) {
                  return false;
                }

                sink.write(result->body.data(), result->body.size());
                break;
              }
            }
          }

          sink.done();
          return true;
        });
  });

  {  // NOTE: Call register API on CMMU
    json j_body = json::object();
    j_body["port"] = port;
    auto result = cmmu.Post("/register", j_body.dump(), "application/json");
    if (!result) {
      std::cerr << "Failed to register to CMMU: CMMU host (TODO FILL THIS WITH "
                   "ADDRESS) is down"
                << std::endl;
      return 1;
    }

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

  // TODO: Add a default exception handler for server
  std::cerr << "Agent is listening at " << host << ":" << port << std::endl;
  server.listen(host, port);

  return 0;
}
