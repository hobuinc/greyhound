set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(defs_hpp_in "${CMAKE_CURRENT_SOURCE_DIR}/defs.hpp.in")
set(defs_hpp    "${CMAKE_CURRENT_BINARY_DIR}/defs.hpp")

configure_file(${defs_hpp_in} ${defs_hpp})

set(HEADERS
    "${BASE}/app.hpp"
    "${BASE}/auth.hpp"
    "${BASE}/chunker.hpp"
    "${BASE}/configuration.hpp"
    "${BASE}/manager.hpp"
    "${BASE}/resource.hpp"
    "${BASE}/router.hpp"
)

set(SOURCES
    "${BASE}/app.cpp"
    "${BASE}/auth.cpp"
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
target_link_libraries(app dl pthread)

if (${GREYHOUND_OPENSSL})
    target_link_libraries(app ${OPENSSL_LIBRARIES})
    target_include_directories(app PRIVATE "${OPENSSL_INCLUDE_DIR}")
endif()

set_target_properties(app PROPERTIES OUTPUT_NAME greyhound)
install(TARGETS app DESTINATION bin)

install(FILES ${HEADERS} DESTINATION include/greyhound/)
install(FILES ${defs_hpp} DESTINATION include/greyhound/)
install(DIRECTORY "${BASE}/public" DESTINATION include/greyhound)

