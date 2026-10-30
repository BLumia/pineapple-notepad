# PnDependencies.cmake
#
# SPDX-FileCopyrightText: 2026 Gary Wang <git@blumia.net>
#
# Central place for declaring third-party dependencies fetched via
# FetchContent. All dependency metadata (url, sha256, version, ...)
# lives in dependencies.json at the repository root; this module turns
# the matching entry into a single FetchContent_Declare() call, so the
# Find*.cmake modules never hardcode versions or hashes.
#
# dependencies.json is a flatpak-builder source array, so the flatpak
# manifest can reference the file directly from its `sources:` list
# (flatpak-builder ignores the extension keys below). The x-pn-* keys
# are pineapple-notepad extensions read by this module:
#
#   x-pn-name                FetchContent content name (unique key)
#   x-pn-version             dependency version (display + fallback git tag)
#   x-pn-git                 online source fetched via git instead of url:
#                            { "repository": <url>, "tag": <optional>,
#                              "shallow": <bool, default true> }
#   x-pn-find-package-names  prefer find_package() when it can satisfy
#                            the dependency (FIND_PACKAGE_ARGS NAMES ...)
#   x-pn-exclude-from-all    add EXCLUDE_FROM_ALL to the declaration
#   x-pn-source-subdir       SOURCE_SUBDIR, only for the rare case where
#                            CMakeLists.txt is not at the top of the
#                            dependency's source tree
#   x-pn-patches             patches (relative to cmake/patches/) applied
#                            via `git apply --ignore-whitespace`
#
# Flatpak keys consumed: type, url, sha256, dest, dest-filename.
#
# Source selection, in order:
#   1. find_package() when x-pn-find-package-names is configured and a
#      matching package is installed (applies in every mode, via
#      FIND_PACKAGE_ARGS);
#   2. the local archive under offline/ through a file:// URL when the
#      offline/ directory exists - a missing archive is a fatal error so
#      a stale cache after a version bump fails loudly instead of
#      silently falling back to the network;
#   3. the online source otherwise (git when x-pn-git is given, else
#      the archive url).
#
# Functions:
#   pn_declare_dependency(<name>)  - declare <name> for FetchContent
#   pn_get_version(<name> <out>)   - get the x-pn-version of <name>

include_guard(GLOBAL)

include(FetchContent)

# Repository root (parent of this cmake/ directory). Cached so it stays
# visible from every scope that ends up calling the functions below.
get_filename_component(_pn_source_root "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
set(PN_SOURCE_ROOT "${_pn_source_root}" CACHE INTERNAL "pineapple-notepad repository root")
unset(_pn_source_root)

function(_pn_dependencies_json out_var)
    get_property(cached GLOBAL PROPERTY _PN_DEPENDENCIES_JSON)
    if(NOT cached)
        if(NOT EXISTS "${PN_SOURCE_ROOT}/dependencies.json")
            message(FATAL_ERROR "dependencies.json not found at ${PN_SOURCE_ROOT}")
        endif()
        file(READ "${PN_SOURCE_ROOT}/dependencies.json" raw)
        set_property(GLOBAL PROPERTY _PN_DEPENDENCIES_JSON "${raw}")
        set(cached "${raw}")
    endif()
    set(${out_var} "${cached}" PARENT_SCOPE)
endfunction()

function(_pn_find_entry name out_var)
    _pn_dependencies_json(json)
    string(JSON count LENGTH "${json}")
    set(i 0)
    while(i LESS count)
        string(JSON entry GET "${json}" ${i})
        set(entry_name "")
        string(JSON entry_name ERROR_VARIABLE name_err GET "${entry}" "x-pn-name")
        if(name_err)
            message(FATAL_ERROR "Entry ${i} in dependencies.json has no x-pn-name key.")
        endif()
        if(entry_name STREQUAL name)
            set(${out_var} "${entry}" PARENT_SCOPE)
            return()
        endif()
        math(EXPR i "${i} + 1")
    endwhile()
    message(FATAL_ERROR
        "dependencies.json has no entry with x-pn-name '${name}'. "
        "Add one, or fix the name passed to pn_declare_dependency().")
endfunction()

# Translate a JSON boolean (returned by string(JSON GET) as text) into a
# CMake truthiness value.
function(_pn_json_bool value out_var)
    if(value STREQUAL "true" OR value STREQUAL "1" OR value STREQUAL "ON")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(pn_get_version name out_var)
    _pn_find_entry("${name}" entry)
    set(version "")
    string(JSON version ERROR_VARIABLE err GET "${entry}" "x-pn-version")
    if(err OR NOT version)
        message(FATAL_ERROR "Entry '${name}' in dependencies.json has no x-pn-version.")
    endif()
    set(${out_var} "${version}" PARENT_SCOPE)
endfunction()

function(pn_declare_dependency name)
    _pn_find_entry("${name}" entry)

    string(JSON url GET "${entry}" "url")
    string(JSON sha256 GET "${entry}" "sha256")

    # 'dest' is flatpak-only, but keep it honest: everything expects the
    # archives to be staged into offline/.
    set(dest "")
    string(JSON dest ERROR_VARIABLE _dest_err GET "${entry}" "dest")
    if(dest AND NOT dest STREQUAL "offline")
        message(FATAL_ERROR "Entry '${name}' in dependencies.json must use \"dest\": \"offline\".")
    endif()

    # Archive filename inside offline/: dest-filename when given (needed
    # for e.g. GitHub auto-generated archives whose url basename is just
    # the tag), else the basename of the url.
    set(filename "")
    string(JSON dest_filename ERROR_VARIABLE _df_err GET "${entry}" "dest-filename")
    if(dest_filename)
        set(filename "${dest_filename}")
    else()
        get_filename_component(filename "${url}" NAME)
    endif()

    set(source_args "")

    set(offline_dir "${PN_SOURCE_ROOT}/offline")
    if(EXISTS "${offline_dir}")
        if(EXISTS "${offline_dir}/${filename}")
            list(APPEND source_args
                URL "file://${offline_dir}/${filename}"
                URL_HASH "SHA256=${sha256}")
            message(STATUS "Dependency ${name}: FetchContent fallback source is offline archive ${filename}")
        else()
            message(FATAL_ERROR
                "Dependency ${name}: the offline/ directory exists but "
                "${filename} is missing (expected from ${url}).\n"
                "Run './build.ps1 -PrepareOffline' to refresh the offline cache, "
                "or remove the offline/ directory to fetch from the network.")
        endif()
    else()
        set(git_spec "")
        string(JSON git_spec ERROR_VARIABLE _git_err GET "${entry}" "x-pn-git")
        if(git_spec AND NOT _git_err)
            string(JSON repository GET "${git_spec}" "repository")
            set(tag "")
            string(JSON tag ERROR_VARIABLE _tag_err GET "${git_spec}" "tag")
            if(NOT tag)
                # Fall back to the declared version; works for projects
                # whose tags are plain version numbers.
                string(JSON tag ERROR_VARIABLE _tag2_err GET "${entry}" "x-pn-version")
            endif()
            set(shallow TRUE)
            set(shallow_raw "")
            string(JSON shallow_raw ERROR_VARIABLE _shallow_err GET "${git_spec}" "shallow")
            if(shallow_raw)
                _pn_json_bool("${shallow_raw}" shallow)
            endif()
            list(APPEND source_args GIT_REPOSITORY "${repository}")
            if(tag)
                list(APPEND source_args GIT_TAG "${tag}")
            endif()
            if(shallow)
                list(APPEND source_args GIT_SHALLOW TRUE)
            endif()
            message(STATUS "Dependency ${name}: FetchContent fallback source is git ${repository} (tag ${tag})")
        else()
            list(APPEND source_args URL "${url}" URL_HASH "SHA256=${sha256}")
            message(STATUS "Dependency ${name}: FetchContent fallback source is ${url}")
        endif()
    endif()

    # Prefer a find_package()-provided package when one is installed,
    # regardless of the online/offline decision made above. The declared
    # x-pn-version is passed as a minimum version, so a stale package
    # installed into our own install prefix (CMAKE_INSTALL_PREFIX is on
    # find_package's default search path) no longer shadows the newer
    # FetchContent source after a version bump.
    set(fpn_raw "")
    string(JSON fpn_raw ERROR_VARIABLE _fpn_err GET "${entry}" "x-pn-find-package-names")
    if(fpn_raw AND NOT _fpn_err)
        string(JSON fpn_count ERROR_VARIABLE _fpn_len_err LENGTH "${fpn_raw}")
        if(_fpn_len_err)
            message(FATAL_ERROR "Entry '${name}': x-pn-find-package-names must be an array.")
        endif()
        set(find_args "")
        set(dep_version "")
        string(JSON dep_version ERROR_VARIABLE _fpv_err GET "${entry}" "x-pn-version")
        if(dep_version AND NOT _fpv_err)
            list(APPEND find_args "${dep_version}")
        endif()
        list(APPEND find_args NAMES)
        set(i 0)
        while(i LESS fpn_count)
            string(JSON n GET "${fpn_raw}" ${i})
            list(APPEND find_args "${n}")
            math(EXPR i "${i} + 1")
        endwhile()
        list(APPEND source_args FIND_PACKAGE_ARGS ${find_args})
    endif()

    set(efa "")
    string(JSON efa ERROR_VARIABLE _efa_err GET "${entry}" "x-pn-exclude-from-all")
    if(efa)
        _pn_json_bool("${efa}" use_exclude)
        if(use_exclude)
            list(APPEND source_args EXCLUDE_FROM_ALL)
        endif()
    endif()

    # x-pn-source-subdir is only for the rare case where the dependency's
    # CMakeLists.txt does not live at the top of its source tree. Note that
    # FetchContent already promotes a lone top-level directory of an
    # extracted archive (e.g. GitHub's <repo>-<tag>/) to the source root,
    # so it must NOT be set just to account for such a wrapper directory.
    set(subdir "")
    string(JSON subdir ERROR_VARIABLE _subdir_err GET "${entry}" "x-pn-source-subdir")
    if(subdir)
        list(APPEND source_args SOURCE_SUBDIR "${subdir}")
    endif()

    set(patches_raw "")
    string(JSON patches_raw ERROR_VARIABLE _patches_err GET "${entry}" "x-pn-patches")
    if(patches_raw AND NOT _patches_err)
        string(JSON patches_count ERROR_VARIABLE _pl_err LENGTH "${patches_raw}")
        if(_pl_err)
            message(FATAL_ERROR "Entry '${name}': x-pn-patches must be an array.")
        endif()
        set(patch_command git apply --ignore-whitespace)
        set(i 0)
        while(i LESS patches_count)
            string(JSON p GET "${patches_raw}" ${i})
            list(APPEND patch_command "${PN_SOURCE_ROOT}/cmake/patches/${p}")
            math(EXPR i "${i} + 1")
        endwhile()
        list(APPEND source_args PATCH_COMMAND ${patch_command})
    endif()

    FetchContent_Declare(${name}
        ${source_args}
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
    )
endfunction()
