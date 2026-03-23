if (NOT DEFINED SRC OR NOT DEFINED DST)
    message(FATAL_ERROR "sync_resources.cmake requires SRC and DST")
endif()

if (NOT IS_DIRECTORY "${SRC}")
    message(FATAL_ERROR "Resource source does not exist: ${SRC}")
endif()

if (EXISTS "${DST}")
    file(REMOVE_RECURSE "${DST}")
endif()

file(MAKE_DIRECTORY "${DST}")
file(COPY "${SRC}/" DESTINATION "${DST}")
