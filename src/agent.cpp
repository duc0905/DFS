#include <httplib.h>
#include <iostream>

int main() {
  std::cerr << "Hello from agent" << std::endl;

  httplib::Server server;

  server.Get("/", [](const httplib::Request& req, httplib::Response& res) {
    std::cerr << "Got req from " << req.local_addr << " | " << req.remote_addr
              << std::endl;
    res.set_content("{\"hello\": \"world\"}", "application/json");
    res.status = httplib::StatusCode::OK_200;
  });

  std::cerr << "Server is listening at 0.0.0.0:4321" << std::endl;
  server.listen("0.0.0.0", 4321);
  std::cerr << "After listen..." << std::endl;

  return 0;
}
