cmake_minimum_required(VERSION 3.12.0)
project(passwordchecker VERSION 0.1.0 LANGUAGES C)

option (USE_ADWAITA "Use Adwaita theme" ON)
if (USE_ADWAITA)
    add_definitions(-DUSE_ADWAITA)
endif()

find_package (PkgConfig REQUIRED)
pkg_check_modules (GIO REQUIRED gio-2.0)
pkg_check_modules (GTK REQUIRED gtk4)
pkg_check_modules (WBCLIENT REQUIRED wbclient)
if (USE_ADWAITA)
    pkg_check_modules (ADWAITA REQUIRED libadwaita-1)
endif ()

set (CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/passwordchecker/)

set (GSCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/../data/org.altlinux.passwordchecker.gschema.xml)

set (SOURCES ${CMAKE_CURRENT_SOURCE_DIR}passwordchecker.c
             ${CMAKE_CURRENT_SOURCE_DIR}passwordchecker-ldap.c
             ${CMAKE_CURRENT_SOURCE_DIR}winbind-helper.c)
set (SYSTEMD_UNIT ${CMAKE_CURRENT_SOURCE_DIR}/../data/passwordchecker-user.service)
set (DESKTOP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../data/org.altlinux.PasswordCheckerSettings.desktop)

set (UI_APP_NAME "PasswordCheckerSettings")
set (SOURCES_UI ${CMAKE_CURRENT_SOURCE_DIR}passwordchecker-ui.c)

if (USE_ADWAITA)
    set (UI_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../data/ui/passwordchecker-gnome-page-app.ui
                  ${CMAKE_CURRENT_SOURCE_DIR}/../data/ui/passwordchecker-gnome-page-con.ui
                  ${CMAKE_CURRENT_SOURCE_DIR}/../data/ui/passwordchecker-gnome-switcher.ui
                  ${CMAKE_CURRENT_SOURCE_DIR}/../data/ui/passwordchecker-gnome-window.ui)
else ()
    set (UI_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../data/ui/page_application.glade
                  ${CMAKE_CURRENT_SOURCE_DIR}/../data/ui/page_connection.glade)
endif ()

set (ICON_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../data/icons/error.svg)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
add_executable(${UI_APP_NAME} ${SOURCES_UI})

add_subdirectory(locales)

find_path (LIBLDAP_INCLUDE_DIR 
           NAMES ldap.h
           PATHS /usr/include)
find_path (SASL_INCLUDE_DIR 
           NAMES sasl.h
           PATHS /usr/include/sasl)
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
elseif (NOT SASL_INCLUDE_DIR)
    message (FATAL_ERROR "SASL_INCLUDE_DIR not found!")
else()
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${LIBLDAP_INCLUDE_DIR} ${SASL_INCLUDE_DIR} ${GIO_INCLUDE_DIRS} ${WBCLIENT_INCLUDE_DIRS})
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LIBLDAP_LIBRARY}  ${LIBLBER_LIBRARY} ${GIO_LIBRARIES} ${WBCLIENT_LIBRARIES})
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE ${GIO_CFLAGS} ${WBCLIENT_CFLAGS})
endif()

target_include_directories(${UI_APP_NAME} PRIVATE ${GTK_INCLUDE_DIRS})
target_link_libraries(${UI_APP_NAME} PRIVATE ${GTK_LIBRARIES})
target_compile_options(${UI_APP_NAME} PRIVATE ${GTK_CFLAGS})
if (USE_ADWAITA)
    target_include_directories(${UI_APP_NAME} PRIVATE ${ADWAITA_INCLUDE_DIRS})
    target_link_libraries(${UI_APP_NAME} PRIVATE ${ADWAITA_LIBRARIES})
    target_compile_options(${UI_APP_NAME} PRIVATE ${ADWAITA_CFLAGS})
endif ()

install (TARGETS ${UI_APP_NAME}
         DESTINATION /usr/bin)

install (FILES ${UI_FILES}
         DESTINATION /usr/share/${UI_APP_NAME}/ui)

install (FILES ${ICON_FILES}
         DESTINATION /usr/share/${UI_APP_NAME}/icons)

install (TARGETS ${CMAKE_PROJECT_NAME}
         DESTINATION /usr/bin)

install (FILES ${SYSTEMD_UNIT}
         DESTINATION /usr/lib/systemd/user)

install (FILES ${GSCHEMA}
         DESTINATION /usr/share/glib-2.0/schemas)

install (FILES ${DESKTOP_FILE}
         DESTINATION /usr/share/applications)