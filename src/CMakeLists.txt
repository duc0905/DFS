find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(stduuid CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)

set(CMMU_SRC_FILES 
	"cmmu.cpp"
)
add_executable(CMMU ${CMMU_SRC_FILES})
target_link_libraries(CMMU PRIVATE httplib::httplib)
target_link_libraries(CMMU PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(CMMU PRIVATE stduuid)
target_link_libraries(CMMU PRIVATE argparse::argparse)

set(Agent_SRC_FILES
	"agent.cpp"
)
add_executable(Agent ${Agent_SRC_FILES})
target_link_libraries(Agent PRIVATE httplib::httplib)
target_link_libraries(Agent PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(Agent PRIVATE argparse::argparse)
