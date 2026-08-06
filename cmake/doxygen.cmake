# Into .cache, which is ignored, rather than into the build directory. There is more than one of
# those, and the point of a fixed path is that whatever serves the pages does not have to be told
# which one was configured last. Nothing downstream reaches this: the option is off by default and
# a consumer of the library never turns it on.
set(_doxy_dir ${CMAKE_CURRENT_SOURCE_DIR}/.cache)

# Only the settings that differ from Doxygen's defaults. Everything left out keeps whatever the
# version in use considers normal, which is what keeps this readable.
set(_doxy_content "PROJECT_NAME           = ${PROJECT_NAME}
PROJECT_NUMBER         = ${PROJECT_VERSION}
PROJECT_BRIEF          = \"${DOXY_DESCRIPTION}\"
OUTPUT_DIRECTORY       = ${_doxy_dir}
HTML_OUTPUT            = docs
GENERATE_LATEX         = NO

# The public headers and nothing else. src/ holds the implementation and the private _p.h
# headers, which are deliberately not part of what anybody reads.
INPUT                  = ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/README.md
USE_MDFILE_AS_MAINPAGE = ${CMAKE_CURRENT_SOURCE_DIR}/README.md
RECURSIVE              = YES
STRIP_FROM_INC_PATH    = ${CMAKE_CURRENT_SOURCE_DIR}/include
STRIP_FROM_PATH        = ${CMAKE_CURRENT_SOURCE_DIR}

# This library writes no \\brief anywhere, on purpose. Without this the summary tables at the top
# of every page come out blank and only the detailed sections carry anything.
JAVADOC_AUTOBRIEF      = YES

# The whole public surface, documented or not. A reference that silently drops isNull() because
# the name says enough already is a reference nobody can check anything against.
EXTRACT_ALL            = YES
EXTRACT_STATIC         = YES
HIDE_UNDOC_MEMBERS     = NO
SORT_MEMBER_DOCS       = NO

# Doxygen is not a compiler and does not see the configure step, so the switches have to be
# spelled out. Left alone, the export attribute becomes part of every class name and the platform
# headers document only whichever branch it guessed at.
ENABLE_PREPROCESSING   = YES
MACRO_EXPANSION        = YES
EXPAND_ONLY_PREDEF     = YES
PREDEFINED             = STDC_EXPORT= STDC_DECL_EXPORT= STDC_DECL_IMPORT= STDC_EXCEPTIONS=1 _WIN32=1 DOXYGEN=1

# Undocumented is a choice here rather than an oversight, so it is not worth hearing about each
# time. The rest stay on: a \\param naming an argument that is gone is worth a warning.
WARN_IF_UNDOCUMENTED   = NO

GENERATE_TREEVIEW      = YES
DISABLE_INDEX          = NO
FULL_SIDEBAR           = NO
HTML_COLORSTYLE        = LIGHT
")

set(_doxy_file ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_Doxyfile)
file(WRITE ${_doxy_file} ${_doxy_content})

# Not part of ALL. Documentation is asked for by name, and Doxygen is slow enough that nobody
# wants it on the path from an edit to a test run.
add_custom_target(${PROJECT_NAME}_docs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_doxy_dir}
    COMMAND ${DOXYGEN_EXECUTABLE} ${_doxy_file}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating documentation into ${_doxy_dir}/docs"
    VERBATIM
)
