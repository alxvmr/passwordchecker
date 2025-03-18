set (DOMAIN passwordchecker)
 set (LINGUAS ru)
 set (MO_FILES)
 
 foreach (LANG ${LINGUAS})
     set (MSGFMT_INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${LANG}/${DOMAIN}.po)
     set (MSGFMT_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${LANG}.mo)
 
     add_custom_command(
         OUTPUT ${MSGFMT_OUTPUT_FILE}
         COMMAND msgfmt --output-file=${MSGFMT_OUTPUT_FILE} ${MSGFMT_INPUT_FILE}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
         DEPENDS ${MSGFMT_INPUT_FILE}
         COMMENT "Compiling ${LANG} translation"
     )
 
     list (APPEND MO_FILES ${MSGFMT_OUTPUT_FILE})
 
     install (FILES ${MSGFMT_OUTPUT_FILE}
              DESTINATION /usr/share/locale/${LANG}/LC_MESSAGES
              RENAME ${DOMAIN}.mo)
 endforeach ()
 
 if (MO_FILES)
     add_custom_target (${DOMAIN}-po
         ALL
         DEPENDS ${MO_FILES})
 endif ()