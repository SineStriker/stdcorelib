# Laid out under out/ the way it will be laid out once installed, beside bin and lib, so that
# installing it is a copy.
set(_doxy_dir ${CMAKE_CURRENT_BINARY_DIR}/out/share/doc/${STDC_INSTALL_NAME})

# Only the settings that differ from Doxygen's defaults. Everything left out keeps the default
# of whichever Doxygen runs.
set(_doxy_content "PROJECT_NAME           = ${PROJECT_NAME}
PROJECT_NUMBER         = ${PROJECT_VERSION}
PROJECT_BRIEF          = \"${DOXY_DESCRIPTION}\"
OUTPUT_DIRECTORY       = ${_doxy_dir}
HTML_OUTPUT            = html
GENERATE_LATEX         = NO

# The public headers and nothing else. src/ holds the implementation and the private _p.h
# headers.
INPUT                  = ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/README.md
USE_MDFILE_AS_MAINPAGE = ${CMAKE_CURRENT_SOURCE_DIR}/README.md
RECURSIVE              = YES
STRIP_FROM_INC_PATH    = ${CMAKE_CURRENT_SOURCE_DIR}/include
STRIP_FROM_PATH        = ${CMAKE_CURRENT_SOURCE_DIR}

# This library writes no \\brief anywhere. Without this the summary table at the top of every
# page is empty.
JAVADOC_AUTOBRIEF      = YES

# The whole public surface, documented or not. An entity whose name says enough still belongs
# in a reference.
EXTRACT_ALL            = YES
EXTRACT_STATIC         = YES
HIDE_UNDOC_MEMBERS     = NO
SORT_MEMBER_DOCS       = NO

# Doxygen does not see the configure step, so the switches are spelled out here. Without them
# the export attribute reads as part of every class name, and the platform headers document
# whichever branch Doxygen guessed at.
ENABLE_PREPROCESSING   = YES
MACRO_EXPANSION        = YES
EXPAND_ONLY_PREDEF     = YES
#
# STDC_ALLOCA is not one Doxygen can work out. vla.h defines it from _MSC_VER or __GNUC__,
# neither of which is set here, so everything under the #ifdef below it was missing from the
# output and nothing said so.
PREDEFINED             = STDC_EXPORT= STDC_DECL_EXPORT= STDC_DECL_IMPORT= STDC_EXCEPTIONS=1 STDC_ALLOCA(size)= _WIN32=1 DOXYGEN=1

# Undocumented is deliberate here, so it is not worth a warning each time. The rest stay on. A
# \\param naming an argument that no longer exists is worth hearing about.
WARN_IF_UNDOCUMENTED   = NO

# The pages are still written, and then the run reports failure. Markup that does not say what
# it meant to say is a defect in the documentation.
WARN_AS_ERROR          = FAIL_ON_WARNINGS

# Set explicitly, because the default is not the same across Doxygen versions. 1.9.8 on a
# runner went looking for Graphviz and failed the build with exit code 127, where 1.10 here had
# never asked for it. Turning it on means installing graphviz wherever this runs.
HAVE_DOT               = NO

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
    COMMENT "Generating documentation into ${_doxy_dir}/html"
    VERBATIM
)

# Optional, because the target is outside ALL: an install that was never asked for documentation
# should not fail over its absence.
if(STDC_INSTALL)
    install(DIRECTORY ${_doxy_dir}/ DESTINATION ${CMAKE_INSTALL_DOCDIR} OPTIONAL)
endif()
