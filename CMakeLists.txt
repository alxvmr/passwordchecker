cmake_minimum_required(VERSION 3.5.0)
project(passwordchecker VERSION 0.1.0 LANGUAGES C)

find_package (PkgConfig REQUIRED)
pkg_check_modules (GIO REQUIRED gio-2.0)

set (SOURCES passwordchecker.c passwordchecker-ldap.c)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

find_path (LIBLDAP_INCLUDE_DIR 
           NAMES ldap.h
           PATHS /usr/local/include /usr/include)
find_library (LIBLDAP_LIBRARY
              NAMES libldap.so
              PATHS /usr/lib64)
find_library (LIBLBER_LIBRARY
              NAMES liblber.so
              PATHS /usr/lib64)

if(NOT LIBLDAP_INCLUDE_DIR OR NOT LIBLDAP_LIBRARY)
    message (FATAL_ERROR "libldap not found!")
elseif (NOT LIBLBER_LIBRARY)
    message (FATAL_ERROR "liblber not found!")
else()
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${LIBLDAP_INCLUDE_DIR} ${GIO_INCLUDE_DIRS})
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LIBLDAP_LIBRARY}  ${LIBLBER_LIBRARY} ${GIO_LIBRARIES})
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE ${GIO_CFLAGS})
endif()
