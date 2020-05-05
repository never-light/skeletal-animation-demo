if(${CMAKE_GENERATOR} MATCHES "Unix Makefiles")
    # TODO: add wall and werror flags
elseif(MSVC)
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/W4>)
#    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/WX>)
endif()

file(GLOB_RECURSE GAME_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

file(GLOB_RECURSE GAME_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        )

SET(GAME_SOURCES ${GAME_SOURCES} ${GAME_INCLUDES})

add_executable(game ${GAME_SOURCES})

target_include_directories(game PUBLIC ../)
target_include_directories(game PRIVATE ./)

target_include_directories(game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        ../ThirdParty/OpenGL)

target_link_libraries(game ${CONAN_LIBS})
