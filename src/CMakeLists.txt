find_package(httplib CONFIG REQUIRED)

set(CMMU_SRC_FILES 
	"cmmu.cpp"
)

add_executable(CMMU ${CMMU_SRC_FILES})

target_link_libraries(CMMU PRIVATE httplib::httplib)
