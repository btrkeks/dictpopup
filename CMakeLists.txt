cmake_minimum_required(VERSION 3.5)
project(dictpopup C)

include(GNUInstallDirs)
include(CTest)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(NOTIFY REQUIRED libnotify)
find_package(LibZip REQUIRED)
find_package(CURL REQUIRED)

set(CMAKE_C_COMPILER gcc)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    set(CMAKE_C_FLAGS
            "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wdouble-promotion -Wshadow \
	     -Wno-unused-parameter -Wno-sign-conversion -Wno-unused-function -Wpointer-arith \
	     -Wmissing-prototypes -Wstrict-prototypes -Wstrict-overflow -Wcast-align \
	     -O0 -ggdb")
    set(CMAKE_C_FLAGS
            "${CMAKE_C_FLAGS} -fsanitize=address,undefined -fsanitize-undefined-trap-on-error -fstack-protector-strong")
else ()
    add_compile_options(-Ofast -flto -march=native -mtune=native -g)
endif ()

add_definitions(${GTK3_CFLAGS_OTHER} ${NOTIFY_CFLAGS_OTHER}
        ${LIBZIP_DEFINITIONS} ${CURL_DEFINITIONS})
set(SRCS
        src/ankiconnectc.c
        src/db.c
        src/deinflector.c
        src/dictpopup.c
        src/platformdep.c
        src/settings.c
        src/util.c)

set(JPPRON_SRCS
        src/jppron/jppron.c
        src/jppron/database.c
        src/jppron/ajt_audio_index_parser.c
	src/yyjson.c)

# ##############################################################################
# dictpopup
# ##############################################################################
add_executable(dictpopup src/frontends/gtk3popup.c ${SRCS} ${JPPRON_SRCS})
target_compile_definitions(dictpopup PUBLIC NOTIFICATIONS)
target_include_directories(
        dictpopup PRIVATE ${GTK3_INCLUDE_DIRS} ${NOTIFY_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS} include/)
target_link_directories(dictpopup PRIVATE ${GTK3_LIBRARY_DIRS}
        ${NOTIFY_LIBRARY_DIRS} ${CURL_LIBRARY_DIRS})
target_link_libraries(dictpopup PRIVATE ${GTK3_LIBRARIES} ${NOTIFY_LIBRARIES}
        ${CURL_LIBRARIES} -lmecab -llmdb)

## X11 window title support
target_compile_definitions(dictpopup PUBLIC HAVEX11)
target_link_libraries(dictpopup PRIVATE -lXfixes -lX11)

# ##############################################################################
# dictpopup-create
# ##############################################################################
add_executable(dictpopup-create src/dictpopup_create.c ${SRCS}
	src/yyjson.c
        src/yomichan_parser.c)
target_include_directories(
        dictpopup-create PRIVATE ${GTK3_INCLUDE_DIRS} ${LIBZIP_INCLUDE_DIRS} include/)
target_link_directories(dictpopup-create PRIVATE ${GTK3_LIBRARY_DIRS}
        ${LIBZIP_LIBRARY_DIRS} ${CURL_LIBRARY_DIRS})
target_link_libraries(dictpopup-create PRIVATE ${GTK3_LIBRARIES} -lzip -llmdb
        ${CURL_LIBRARIES} -lmecab)

# ##############################################################################
# cli
# ##############################################################################
add_executable(cli EXCLUDE_FROM_ALL src/frontends/cli.c ${SRCS})
target_include_directories(
        cli PRIVATE ${GTK3_INCLUDE_DIRS} ${NOTIFY_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS} include/)
target_link_directories(cli PRIVATE ${GTK3_LIBRARY_DIRS}
        ${NOTIFY_LIBRARY_DIRS} ${CURL_LIBRARY_DIRS})
target_link_libraries(cli PRIVATE ${GTK3_LIBRARIES} ${NOTIFY_LIBRARIES}
        ${CURL_LIBRARIES} -lmecab -llmdb)

# ##############################################################################
# Tests
# ##############################################################################
if (BUILD_TESTING)
    enable_testing()
    find_package(cgreen REQUIRED)
    add_executable(
            c_tests
            tests/main.c
            tests/deinflector_tests.c
            tests/ankiconnect_tests.c
            src/deinflector.c
            src/util.c
            tests/yomichan_parser_tests.c
            src/db.c
            src/platformdep.c
            src/yyjson.c
            tests/ajt_audio_index_parser_tests.c)
    target_compile_definitions(c_tests PRIVATE UNIT_TEST)
    target_include_directories(c_tests PRIVATE ${GTK3_INCLUDE_DIRS} ${CGREEN_INCLUDE_DIRS} include/ src/ src/jppron)
    target_link_libraries(
            c_tests
            PRIVATE ${CGREEN_LIBRARIES}
            ${GTK3_LIBRARIES}
            ${NOTIFY_LIBRARIES}
            ${CURL_LIBRARIES}
            -llmdb
            -lmecab
            -lzip)
    add_test(NAME run_c_tests COMMAND $<TARGET_FILE:c_tests>)

    #    add_test(NAME run_shell_test COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/full_cycle_test.sh)
endif ()

# ##############################################################################
# Install
# ##############################################################################
install(TARGETS dictpopup dictpopup-create DESTINATION bin)
install(FILES config.ini DESTINATION share/dictpopup)
install(FILES data.mdb DESTINATION share/dictpopup)
install(
        DIRECTORY man1/
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
        FILES_MATCHING
        PATTERN "*.1")

# ##############################################################################
# Create DEB
# ##############################################################################
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "butterkeks")
set(CPACK_PACKAGE_VERSION 0.1.5)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "curl,libgtk-3-0,libzip4,mecab,libnotify4,liblmdb-dev")
set(CPACK_PACKAGE_CONTACT "butterkeks@fedora.email")
include(CPack)