cmake_minimum_required(VERSION 3.22)

project(RitualCodec
    VERSION 0.2.0
    DESCRIPTION "Ritual Codec audio effect"
    LANGUAGES C CXX)

include(CTest)

set(RITUAL_JUCE_VERSION "9.0.2" CACHE INTERNAL
    "JUCE version required by Ritual Codec")
set(RITUAL_JUCE_COMMIT "72782788ce18c2d4d760b28e0921d6ffc6431102" CACHE INTERNAL
    "Pinned JUCE commit used by FetchContent")

option(RITUAL_BUILD_CLI "Build the ritual-codec command-line renderer" ON)
option(RITUAL_BUILD_VST3 "Build the Ritual Codec VST3 plug-in" ON)
option(RITUAL_BUILD_AU "Build the Ritual Codec Audio Unit (Apple platforms only)" OFF)
option(RITUAL_BUILD_TESTS "Build and register the engine and plug-in tests" ON)

set(RITUAL_JUCE_SOURCE_DIR "" CACHE PATH
    "Use an existing JUCE source tree instead of downloading the pinned commit")

if(RITUAL_BUILD_AU AND NOT APPLE)
    message(FATAL_ERROR
        "RITUAL_BUILD_AU is only supported on Apple platforms; disable it on ${CMAKE_SYSTEM_NAME}.")
endif()

set(_ritual_plugin_formats)
if(RITUAL_BUILD_VST3)
    list(APPEND _ritual_plugin_formats VST3)
endif()
if(RITUAL_BUILD_AU)
    list(APPEND _ritual_plugin_formats AU)
endif()

set(_ritual_have_plugin_sources FALSE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugin/PluginProcessor.cpp"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugin/PluginProcessor.h"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugin/PluginEditor.cpp"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugin/PluginEditor.h")
    set(_ritual_have_plugin_sources TRUE)
endif()

if(_ritual_plugin_formats AND NOT _ritual_have_plugin_sources)
    message(FATAL_ERROR
        "Plugin formats were requested, but plugin/PluginProcessor.{h,cpp} and "
        "plugin/PluginEditor.{h,cpp} are not present.")
endif()

# The CLI and engine tests deliberately do not reach this block when no plugin format is
# requested. This keeps a fast, offline CLI-only build free of JUCE and its transitive tools.
if(_ritual_plugin_formats)
    set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
    set(JUCE_COPY_PLUGIN_AFTER_BUILD OFF CACHE BOOL "" FORCE)

    function(ritual_require_juce_902 juce_source)
        set(_version_header
            "${juce_source}/modules/juce_core/system/juce_StandardHeader.h")
        if(NOT EXISTS "${_version_header}")
            message(FATAL_ERROR
                "RITUAL_JUCE_SOURCE_DIR does not contain JUCE ${RITUAL_JUCE_VERSION}: "
                "missing modules/juce_core/system/juce_StandardHeader.h")
        endif()

        file(READ "${_version_header}" _version_contents)
        foreach(_part MAJOR_VERSION MINOR_VERSION BUILDNUMBER)
            string(REGEX MATCH
                "#define[ \t]+JUCE_${_part}[ \t]+([0-9]+)"
                _version_match "${_version_contents}")
            if(NOT _version_match)
                message(FATAL_ERROR "Unable to read JUCE_${_part} from ${_version_header}")
            endif()
            set(_juce_${_part} "${CMAKE_MATCH_1}")
        endforeach()

        set(_actual_version
            "${_juce_MAJOR_VERSION}.${_juce_MINOR_VERSION}.${_juce_BUILDNUMBER}")
        if(NOT "${_actual_version}" VERSION_EQUAL "${RITUAL_JUCE_VERSION}")
            message(FATAL_ERROR
                "Ritual Codec requires JUCE ${RITUAL_JUCE_VERSION}; found ${_actual_version} "
                "at ${juce_source}.")
        endif()
        set(RITUAL_JUCE_RESOLVED_VERSION "${_actual_version}" PARENT_SCOPE)
    endfunction()

    if(RITUAL_JUCE_SOURCE_DIR)
        get_filename_component(_ritual_juce_source
            "${RITUAL_JUCE_SOURCE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(NOT IS_DIRECTORY "${_ritual_juce_source}")
            message(FATAL_ERROR
                "RITUAL_JUCE_SOURCE_DIR does not name a directory: ${_ritual_juce_source}")
        endif()
        ritual_require_juce_902("${_ritual_juce_source}")
        add_subdirectory("${_ritual_juce_source}" "${CMAKE_BINARY_DIR}/_ritual_juce"
            EXCLUDE_FROM_ALL)
        set(RITUAL_JUCE_SOURCE_DESCRIPTION
            "local source override: ${_ritual_juce_source}")
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG "${RITUAL_JUCE_COMMIT}"
            GIT_SHALLOW FALSE)
        FetchContent_MakeAvailable(JUCE)
        FetchContent_GetProperties(JUCE)
        ritual_require_juce_902("${juce_SOURCE_DIR}")
        set(RITUAL_JUCE_SOURCE_DESCRIPTION
            "FetchContent commit ${RITUAL_JUCE_COMMIT}")
    endif()

    # A single JUCE plug-in target emits whichever formats were selected above. AU is only
    # passed on Apple hosts, which avoids JUCE attempting to configure an unavailable SDK.
    juce_add_plugin(RitualCodec
        COMPANY_NAME "Crrb"
        BUNDLE_ID net.colinraab.ritualcodec
        PRODUCT_NAME "Ritual Codec"
        DESCRIPTION "Ritual Codec audio degradation effect"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE Crrb
        PLUGIN_CODE RtCd
        AU_MAIN_TYPE kAudioUnitType_Effect
        FORMATS ${_ritual_plugin_formats})

    juce_add_binary_data(RitualFonts SOURCES assets/fonts/IBMPlexMono-Regular.ttf assets/fonts/CormorantGaramond.ttf)

    target_sources(RitualCodec PRIVATE
        plugin/PluginProcessor.cpp
        plugin/PluginEditor.cpp)
    target_include_directories(RitualCodec PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin")
    target_compile_features(RitualCodec PRIVATE cxx_std_17)
    target_compile_definitions(RitualCodec PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(RitualCodec PRIVATE
        RitualFonts
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    # Keep the resolved source description in the build tree so release packaging can record
    # the dependency provenance without relying on the caller's shell environment.
    file(WRITE "${CMAKE_BINARY_DIR}/RitualCodecBuildInfo.txt"
        "Ritual Codec ${PROJECT_VERSION}\n"
        "JUCE ${RITUAL_JUCE_RESOLVED_VERSION}\n"
        "${RITUAL_JUCE_SOURCE_DESCRIPTION}\n"
        "VST3: ${RITUAL_BUILD_VST3}\n"
        "AU: ${RITUAL_BUILD_AU}\n")
endif()

option(RITUAL_CLI_SNDFILE "Use installed libsndfile for extended CLI audio input" OFF)

if(RITUAL_BUILD_CLI)
    add_executable(ritual-stream src/rit_main.cpp)
    target_compile_features(ritual-stream PRIVATE cxx_std_17)
    add_executable(ritual-codec src/main.cpp)
    target_compile_features(ritual-codec PRIVATE cxx_std_17)
    target_include_directories(ritual-codec PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    set_target_properties(ritual-codec PROPERTIES OUTPUT_NAME "ritual-codec")
    if(RITUAL_CLI_SNDFILE)
        find_path(RITUAL_SNDFILE_INCLUDE_DIR sndfile.h REQUIRED)
        find_library(RITUAL_SNDFILE_LIBRARY NAMES sndfile REQUIRED)
        foreach(cli ritual-codec ritual-stream)
            target_include_directories(${cli} PRIVATE "${RITUAL_SNDFILE_INCLUDE_DIR}")
            target_link_libraries(${cli} PRIVATE "${RITUAL_SNDFILE_LIBRARY}")
            target_compile_definitions(${cli} PRIVATE RITUAL_CLI_SNDFILE=1)
        endforeach()
        message(STATUS "Extended CLI input enabled; MP3 requires libsndfile 1.1+ with MPEG support")
    endif()

endif()

if(RITUAL_BUILD_TESTS)
    add_executable(ritual-stream-test tests/test_rit_stream.cpp)
    target_include_directories(ritual-stream-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(ritual-stream-test PRIVATE cxx_std_17)
    add_test(NAME ritual-stream COMMAND ritual-stream-test)
    add_executable(ritual-streaming-test tests/test_rit_streaming.cpp)
    target_include_directories(ritual-streaming-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(ritual-streaming-test PRIVATE cxx_std_17)
    add_test(NAME ritual-streaming COMMAND ritual-streaming-test)
    add_executable(ritual-controls-test tests/test_rit_controls.cpp)
    target_compile_features(ritual-controls-test PRIVATE cxx_std_17)
    target_include_directories(ritual-controls-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME ritual-controls COMMAND ritual-controls-test)
    add_executable(ritual-hardening-test tests/test_rit_hardening.cpp)
    target_include_directories(ritual-hardening-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(ritual-hardening-test PRIVATE cxx_std_17)
    add_test(NAME ritual-hardening COMMAND ritual-hardening-test)
    set_tests_properties(ritual-hardening PROPERTIES TIMEOUT 60)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        if(RITUAL_BUILD_CLI)
            set(_audio_input_args "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_audio_input.py" "$<TARGET_FILE_DIR:ritual-stream>")
            if(RITUAL_CLI_SNDFILE)
                list(APPEND _audio_input_args --extended)
            endif()
            add_test(NAME ritual-audio-input COMMAND "${Python3_EXECUTABLE}" ${_audio_input_args})
        endif()
        add_test(NAME ritual-packaging COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_packaging.py")
    endif()
    # The engine test has no dependency on the CLI, but uses a separate target so the
    # RITUAL_BUILD_CLI option remains independent as documented.
    add_executable(ritual-codec-engine-test tests/test_engine.cpp)
    target_compile_features(ritual-codec-engine-test PRIVATE cxx_std_17)
    target_include_directories(ritual-codec-engine-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME ritual-codec-engine COMMAND ritual-codec-engine-test)

    # A processor test is optional until the plug-in test source is supplied. It is only
    # compiled in a JUCE-enabled configuration; a CLI-only configure must remain offline.
    if(_ritual_plugin_formats AND EXISTS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_plugin_processor.cpp")
        add_executable(ritual-codec-plugin-test
            tests/test_plugin_processor.cpp
            plugin/PluginProcessor.cpp
            plugin/PluginEditor.cpp)
        target_compile_features(ritual-codec-plugin-test PRIVATE cxx_std_17)
        target_include_directories(ritual-codec-plugin-test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/plugin")
        target_compile_definitions(ritual-codec-plugin-test PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
        target_link_libraries(ritual-codec-plugin-test PRIVATE
            RitualFonts
        juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
        add_test(NAME ritual-codec-plugin-processor COMMAND ritual-codec-plugin-test)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_plugin_processor.cpp")
        message(STATUS
            "Skipping tests/test_plugin_processor.cpp: enable RITUAL_BUILD_VST3 or RITUAL_BUILD_AU "
            "to provide JUCE in this CLI-only build.")
    endif()
endif()
