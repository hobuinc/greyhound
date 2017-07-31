set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(HEADERS
    "${BASE}/chunker.hpp"
    "${BASE}/configuration.hpp"
    "${BASE}/defs.hpp"
    "${BASE}/manager.hpp"
    "${BASE}/resource.hpp"
    "${BASE}/router.hpp"
)

set(SOURCES
    "${BASE}/configuration.cpp"
    "${BASE}/main.cpp"
    "${BASE}/manager.cpp"
    "${BASE}/resource.cpp"
)

add_executable(app ${SOURCES})

find_package(Boost COMPONENTS system REQUIRED)
target_link_libraries(app ${Boost_LIBRARIES})
target_link_libraries(app jsoncpp)
target_link_libraries(app entwine)
target_link_libraries(app pdalcpp)

if (${GREYHOUND_OPENSSL})
    target_link_libraries(app ${OPENSSL_LIBRARIES})
    target_include_directories(app PRIVATE "${OPENSSL_INCLUDE_DIR}")
endif()

set_target_properties(app PROPERTIES OUTPUT_NAME greyhound)
install(TARGETS app DESTINATION bin)

