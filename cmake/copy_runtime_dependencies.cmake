if(NOT DEFINED EXECUTABLE OR NOT DEFINED SEARCH_DIRECTORY OR NOT DEFINED DESTINATION)
    message(FATAL_ERROR "Runtime dependency copy parameters are incomplete")
endif()

# Do not let DLLs left by an older toolchain satisfy dependency resolution.
file(GLOB EXISTING_RUNTIME_DLLS
    "${DESTINATION}/*.dll"
    "${DESTINATION}/*.DLL"
)
if(EXISTING_RUNTIME_DLLS)
    file(REMOVE ${EXISTING_RUNTIME_DLLS})
endif()

# Inspect the finished PE executable instead of assuming names for a particular
# GCC or MSYS2 release. DIRECTORIES is the active toolchain's bin directory,
# which also contains the UCRT64 RTL-SDR, libusb, and LAME runtime libraries.
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${EXECUTABLE}"
    DIRECTORIES "${SEARCH_DIRECTORY}"
    RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPENDENCIES
    UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPENDENCIES
    PRE_EXCLUDE_REGEXES
        "^api-ms-win-.*\\.dll$"
        "^ext-ms-.*\\.dll$"
    POST_EXCLUDE_REGEXES
        ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
)

if(UNRESOLVED_DEPENDENCIES)
    list(JOIN UNRESOLVED_DEPENDENCIES ", " UNRESOLVED_LIST)
    message(FATAL_ERROR "Unresolved Windows runtime dependencies: ${UNRESOLVED_LIST}")
endif()

foreach(DEPENDENCY IN LISTS RESOLVED_DEPENDENCIES)
    file(COPY "${DEPENDENCY}" DESTINATION "${DESTINATION}")
endforeach()
