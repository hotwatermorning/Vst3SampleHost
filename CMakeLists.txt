cmake_minimum_required(VERSION 3.14)

include(cmake/utils.cmake)

####################################################################
# general setup
####################################################################

message("Generator: ${CMAKE_GENERATOR}")

if(Isx86)
  message(FATAL_ERROR "x86 build is not supported.")
endif()

# Multi configurationをサポートしているGeneratorでビルドする場合であっても、
# Single configurationのみ利用可能にする。
if(CMAKE_CONFIGURATION_TYPES)
  list(LENGTH CMAKE_CONFIGURATION_TYPES len)
  if(len EQUAL 0)
    message(FATAL_ERROR "Please set one build configuration to CMAKE_CONFIGURATION_TYPES.")
  elseif(num GREATER_EQUAL 2)
    message(FATAL_ERROR "Multi configuration is not supported. Please set one build configuration to CMAKE_CONFIGURATION_TYPES.")
  endif()

  set(CMAKE_CONFIGURATION_TYPES ${CMAKE_CONFIGURATION_TYPES} CACHE TYPE INTERNAL FORCE)
  list(GET CMAKE_CONFIGURATION_TYPES 0 CMAKE_BUILD_TYPE)
endif()

message("Build configuration: ${CMAKE_BUILD_TYPE}")
string(TOUPPER "${CMAKE_BUILD_TYPE}" UCONF)
string(TOLOWER "${CMAKE_BUILD_TYPE}" LCONF)
set(BUILD_DIR_NAME "build_${LCONF}")

if("${LCONF}" STREQUAL "debug")
  set(DP "d") # debug postfix
else()
  set(DP "")
endif()

####################################################################
# options
####################################################################
option(ENABLE_BUILD_TESTS "Build test executable" OFF)

####################################################################
# define project
####################################################################
project("Vst3SampleHost")
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

####################################################################
# prepare product info
####################################################################
set(VST3SAMPLEHOST_EXE_ICON_FILE_PATH "${PROJECT_ROOT}/data/app_icon.ico")
set(VST3SAMPLEHOST_BUNDLE_ICON_FILE_NAME "app_icon.icns")
set(VST3SAMPLEHOST_COMPANY_NAME "diatonic.jp")
set(VST3SAMPLEHOST_PRODUCT_NAME "Vst3SampleHost")
set(VST3SAMPLEHOST_VERSION_NUMBER_LIST_BASE 0 9 2)
set(VST3SAMPLEHOST_COPYRIGHT "Copyright 2019, diatonic.jp")
set(VST3SAMPLEHOST_GUI_IDENTIFIER "jp.diatonic.vst3samplehost")

string(JOIN "." VST3SAMPLEHOST_VERSION_STRING ${VST3SAMPLEHOST_VERSION_NUMBER_LIST_BASE})
string(JOIN "," VST3SAMPLEHOST_VERSION_NUMBER_LIST ${VST3SAMPLEHOST_VERSION_NUMBER_LIST_BASE})

configure_file("${PROJECT_ROOT}/src/Resource.rc.in" "${PROJECT_ROOT}/src/AutoGen_Resource.rc")

####################################################################
# update inc files
####################################################################
set(APP_APPNAME_INC "${PROJECT_ROOT}/src/app/AppName.inc")
set(APP_VERSION_INC "${PROJECT_ROOT}/src/app/Version.inc")
set(APP_COMMIT_ID_INC "${PROJECT_ROOT}/src/app/CommitID.inc")

execute_process(
  COMMAND "git" "rev-parse" "HEAD"
  OUTPUT_VARIABLE APP_COMMIT_ID
  RESULT_VARIABLE RESULT_REV_PARSE
  )

if(NOT (RESULT_REV_PARSE EQUAL 0))
  message(FATAL_ERROR "failed to get current commit id: ${RESULT_REV_PARSE}")
endif()

string(STRIP ${APP_COMMIT_ID} APP_COMMIT_ID)

file(WRITE "${APP_APPNAME_INC}" "L\"${VST3SAMPLEHOST_PRODUCT_NAME}\"")
file(WRITE "${APP_VERSION_INC}" "L\"${VST3SAMPLEHOST_VERSION_STRING}\"")
file(WRITE "${APP_COMMIT_ID_INC}" "L\"${APP_COMMIT_ID}\"")

####################################################################
# configure project
####################################################################
project("Vst3SampleHost")
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 17)

get_filename_component(WX_DIR "./ext/wxWidgets" ABSOLUTE)

# add_vst3samplehost_target(<exe_name> [ENABLE_BUILD_TESTS])
function(ADD_VST3SAMPLEHOST_TARGET EXE_NAME)
  cmake_parse_arguments(AVP "ENABLE_BUILD_TESTS" "" "" ${ARGN})

  set(TARGET_NAME "${EXE_NAME}")

  if(NOT ${AVP_ENABLE_BUILD_TESTS})
    set(EXCLUDE_PATTERNS "src/test")
  endif()

  message("Target Name: ${TARGET_NAME}")
  message("Build Test: ${AVP_ENABLE_BUILD_TESTS}")

  set(TARGET_EXTENSIONS "*.c;*.cc;*.cpp;*.h;*.hpp")
  if(IsMSVC)
    list(APPEND TARGET_EXTENSIONS "*.rc")
  elseif(IsXcode)
    list(APPEND TARGET_EXTENSIONS "*.mm")
  endif()

  set(PREFIX_HEADER_PATH "${PROJECT_ROOT}/src/prefix.hpp")

  unset(SOURCE_FILES)

  create_source_list(
    "${PROJECT_ROOT}/src"
    EXTENSIONS "${TARGET_EXTENSIONS}"
    EXCLUDE_PATTERNS "${EXCLUDE_PATTERNS}"
    OUTPUT_VARIABLE SOURCE_FILES
    )

#   message("Files ${SOURCE_FILES}")

  if(${AVP_ENABLE_BUILD_TESTS})
    if(IsMSVC)
      add_executable(${TARGET_NAME} ${SOURCE_FILES})
    elseif(IsXcode)
      add_executable(${TARGET_NAME} MACOSX_BUNDLE ${SOURCE_FILES})
    endif()
  else()
    if(IsMSVC)
      add_executable(${TARGET_NAME} WIN32 ${SOURCE_FILES})
    elseif(IsXcode)
      add_executable(${TARGET_NAME} MACOSX_BUNDLE ${SOURCE_FILES})
    endif()
  endif()

  target_include_directories(
    ${TARGET_NAME}
    PRIVATE
    "./${PROJECT_ROOT}/src"
    "./ext/vst3sdk"
    "./schema/cpp"
    "./ext/portaudio/${BUILD_DIR_NAME}/install/include"
    "./ext/rtmidi/${BUILD_DIR_NAME}/install/include"
    )

  target_compile_definitions(${TARGET_NAME} PRIVATE HAVE_TYPE_TRAITS=1)
  if(${AVP_ENABLE_BUILD_TESTS})
    target_include_directories(${TARGET_NAME} PRIVATE "./ext/Catch2/single_include")
    target_compile_definitions(${TARGET_NAME} PRIVATE ENABLE_BUILD_TESTS)
  endif()

  set_target_properties(${TARGET_NAME} PROPERTIES
    "RUNTIME_OUTPUT_DIRECTORY_${UCONF}"
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${TARGET_NAME}"
    )
  set_target_properties(${TARGET_NAME} PROPERTIES
    OUTPUT_NAME
    "${EXE_NAME}"
    )

  if(IsMSVC)
    target_compile_options(${TARGET_NAME} PRIVATE
      /nologo /MP /source-charset:utf-8
      -DUNICODE -D_UNICODE -DNOMINMAX -D_USE_MATH_DEFINES /EHsc
      -D_SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
      -D_CRT_SECURE_NO_WARNINGS /FI"${PREFIX_HEADER_PATH}"
      )

    if("${LCONF}" STREQUAL "debug")
      target_compile_options(${TARGET_NAME} PRIVATE /MDd -DEBUG -D_DEBUG)
    else()
      target_compile_options(${TARGET_NAME} PRIVATE /MD)
    endif()

    target_include_directories(${TARGET_NAME} PRIVATE
      "${WX_DIR}/${BUILD_DIR_NAME}/install/include"
      "${WX_DIR}/${BUILD_DIR_NAME}/install/lib/vc_x64_lib/mswu${DP}"
      )
    get_wxwidgets_libraries("${WX_DIR}/${BUILD_DIR_NAME}/install/lib/vc_x64_lib" ${CMAKE_BUILD_TYPE} WX_LIBS)
    # message("WX_LIBS: ${WX_LIBS}")
    # target_link_libraries(${TARGET_NAME} PRIVATE ${WX_LIBS})
    target_link_options(${TARGET_NAME} PRIVATE ${WX_LIBS})
  else()
    target_compile_options(${TARGET_NAME} PRIVATE -Werror=return-type -stdlib=libc++)

    if("${LCONF}" STREQUAL "debug")
      target_compile_options(${TARGET_NAME} PRIVATE -DEBUG -D_DEBUG)
    endif()

    get_filename_component(WX_CONFIG_PREFIX "./ext/wxWidgets/build_${LCONF}/install" ABSOLUTE)
    set(WX_CONFIG "${WX_CONFIG_PREFIX}/bin/wx-config")

    if(NOT (EXISTS ${WX_CONFIG}))
      message(FATAL_ERROR "wx-config not found")
    endif()

    execute_process(COMMAND "${WX_CONFIG}" "--prefix=${WX_CONFIG_PREFIX}" "--cxxflags"
      OUTPUT_VARIABLE WX_CXX_FLAGS
      RESULT_VARIABLE RESULT_WX_CONFIG_CXX
      )
    execute_process(COMMAND "${WX_CONFIG}" "--prefix=${WX_CONFIG_PREFIX}" "--libs base,core"
      OUTPUT_VARIABLE WX_LIB_FLAGS
      RESULT_VARIABLE RESULT_WX_CONFIG_LIB
      )

    if((NOT ${RESULT_WX_CONFIG_CXX} EQUAL 0) OR (NOT ${RESULT_WX_CONFIG_LIB} EQUAL 0))
      message(FATAL_ERROR "wx-config failed: [${RESULT_WX_CONFIG_CXX}][${RESULT_WX_CONFIG_LIB}]")
    endif()

    string(STRIP ${WX_CXX_FLAGS} WX_CXX_FLAGS)
    string(STRIP ${WX_LIB_FLAGS} WX_LIB_FLAGS)

    message("WX_CXX_FLAGS ${WX_CXX_FLAGS}")
    message("WX_LIB_FLAGS ${WX_LIB_FLAGS}")
    target_link_options(${TARGET_NAME} PRIVATE "SHELL:${WX_LIB_FLAGS}")
    target_compile_options(${TARGET_NAME} PRIVATE "SHELL:${WX_CXX_FLAGS}")

    target_link_libraries(
      ${TARGET_NAME}
      PRIVATE
      "-framework CoreServices"
      "-framework CoreFoundation"
      "-framework AudioUnit"
      "-framework AudioToolbox"
      "-framework CoreAudio"
      "-framework CoreMidi"
      "-framework CoreGraphics"
      "-framework CoreText"
      "-framework AppKit"
      )

    set_target_properties(${TARGET_NAME} PROPERTIES XCODE_ATTRIBUTE_GCC_PREFIX_HEADER "${PREFIX_HEADER_PATH}")
    set_target_properties(${TARGET_NAME} PROPERTIES XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "YES")

    set_target_properties(${TARGET_NAME} PROPERTIES
      MACOSX_BUNDLE_INFO_STRING "${VST3SAMPLEHOST_COPYRIGHT}"
      MACOSX_BUNDLE_COPYRIGHT "${VST3SAMPLEHOST_COPYRIGHT}"
      MACOSX_BUNDLE_GUI_IDENTIFIER "${VST3SAMPLEHOST_GUI_IDENTIFIER}"
      MACOSX_BUNDLE_BUNDLE_NAME "${TARGET_NAME}"
      MACOSX_BUNDLE_BUNDLE_VERSION ${VST3SAMPLEHOST_VERSION_NUMBER_LIST_BASE}
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${VST3SAMPLEHOST_VERSION_STRING}"
      MACOSX_BUNDLE_LONG_VERSION_STRING "${VST3SAMPLEHOST_VERSION_STRING}"
      MACOSX_BUNDLE_ICON_FILE "${VST3SAMPLEHOST_BUNDLE_ICON_FILE_NAME}"
      )
  endif()

  set(LIB_VST3_SDK_NAME "$<IF:$<CXX_COMPILER_ID:MSVC>,sdk.lib,libsdk.a>")
  set(LIB_VST3_BASE_NAME "$<IF:$<CXX_COMPILER_ID:MSVC>,base.lib,libbase.a>")
  set(LIB_VST3_PLUGINTERFACES_NAME "$<IF:$<CXX_COMPILER_ID:MSVC>,pluginterfaces.lib,libpluginterfaces.a>")
  set(LIB_PORTAUDIO_NAME "$<IF:$<CXX_COMPILER_ID:MSVC>,portaudio_static_x64.lib,libportaudio.a>")
  set(LIB_RTMIDI_NAME "$<IF:$<CXX_COMPILER_ID:MSVC>,rtmidi_static${DP}.lib,librtmidi_static.a>")

  get_filename_component(EXT_DIR "./ext" ABSOLUTE)
  target_link_libraries(
    ${TARGET_NAME}
    PRIVATE
    "${EXT_DIR}/vst3sdk/${BUILD_DIR_NAME}/lib/$<CONFIG>/${LIB_VST3_SDK_NAME}"
    "${EXT_DIR}/vst3sdk/${BUILD_DIR_NAME}/lib/$<CONFIG>/${LIB_VST3_BASE_NAME}"
    "${EXT_DIR}/vst3sdk/${BUILD_DIR_NAME}/lib/$<CONFIG>/${LIB_VST3_PLUGINTERFACES_NAME}"
    "${EXT_DIR}/portaudio/${BUILD_DIR_NAME}/install/lib/${LIB_PORTAUDIO_NAME}"
    "${EXT_DIR}/rtmidi/${BUILD_DIR_NAME}/install/lib/${LIB_RTMIDI_NAME}"
    )

  if(IsXcode)
    find_library(LIB_JACK jack)
    if(LIB_JACK)
      target_link_libraries(${TARGET_NAME} PRIVATE ${LIB_JACK})
    endif()
  endif()

  # Resourceディレクトリに含めるデータのセットアップ
  if(IsXcode)
    get_filename_component(RESOURCE_DIR "./data" ABSOLUTE)
    file(GLOB_RECURSE RESOURCE_FILES "${RESOURCE_DIR}/*")

    # 階層構造を保持したままResourceディレクトリ以下に配置されるように設定する
    foreach(RESOURCE_FILE ${RESOURCE_FILES})
      get_filename_component(FILENAME ${RESOURCE_FILE} NAME)
      if(${FILENAME} STREQUAL ".DS_Store")
        continue()
      endif()

      get_filename_component(DIR ${RESOURCE_FILE} DIRECTORY)
      string(REGEX REPLACE "^${RESOURCE_DIR}" "" DIR_IN_RESOURCE ${DIR})
      target_sources(${TARGET_NAME} PRIVATE ${RESOURCE_FILE})
      set_source_files_properties(${RESOURCE_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources${DIR_IN_RESOURCE}")
      string(REPLACE "/" "\\" GROUP_NAME "Resources${DIR_IN_RESOURCE}")
      source_group(${GROUP_NAME} FILES "${RESOURCE_FILE}")
    endforeach()
  endif()
endfunction()

add_vst3samplehost_target("${PROJECT_NAME}")
add_vst3samplehost_target("${PROJECT_NAME}-Test" ENABLE_BUILD_TESTS)

####################################################################
# configure ccache
####################################################################

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message("Found ccache: ${CCACHE_PROGRAM}")
  # generate wrapper scripts
  function(generate_launcher_script launcher_path compiler_path file_path)
    string(CONCAT launcher_script_template
      "#!/bin/sh\n"
      "# Xcode generator doesn't include the compiler as the\n"
      "# first argument, Ninja and Makefiles do. Handle both cases.\n"
      "if [[ \"$1\" = \"${compiler_path}\" ]] ; then shift; fi\n"
      "export CCACHE_CPP2=true\n"
      "export CCACHE_SLOPPINESS=pch_defines,time_macros\n"
      "exec \"${launcher_path}\" \"${compiler_path}\" \"$@\"\n"
      )
    file(WRITE "${file_path}" "${launcher_script_template}")
    execute_process(COMMAND chmod a+rx "${file_path}")
  endfunction()

  generate_launcher_script("${CCACHE_PROGRAM}" "${CMAKE_C_COMPILER}" "${CMAKE_BINARY_DIR}/launch-c")
  generate_launcher_script("${CCACHE_PROGRAM}" "${CMAKE_CXX_COMPILER}" "${CMAKE_BINARY_DIR}/launch-cxx")

  if(CMAKE_GENERATOR STREQUAL "Xcode")
    # Set Xcode project attributes to route compilation and linking
    # through our scripts
    set(CMAKE_XCODE_ATTRIBUTE_CC "${CMAKE_BINARY_DIR}/launch-c")
    set(CMAKE_XCODE_ATTRIBUTE_CXX "${CMAKE_BINARY_DIR}/launch-cxx")
    set(CMAKE_XCODE_ATTRIBUTE_LD "${CMAKE_BINARY_DIR}/launch-c")
    set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS "${CMAKE_BINARY_DIR}/launch-cxx")
  else()
    # Support Unix Makefiles and Ninja
    set(CMAKE_C_COMPILER_LAUNCHER "${CMAKE_BINARY_DIR}/launch-c")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CMAKE_BINARY_DIR}/launch-cxx")
  endif()
endif()

