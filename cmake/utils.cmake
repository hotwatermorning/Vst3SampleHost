# IsXcodeとIsMSVCを有効にする
if(${CMAKE_GENERATOR} STREQUAL "Xcode")
  set(IsXcode "1")
elseif(${CMAKE_GENERATOR} MATCHES "^Visual Studio .*")
  set(IsMSVC "1")
endif()

# GeneratorがVisual Studioの場合、Win64指定されているかどうかでプラットフォーム指定を分岐
# Xcodeでは常にx64を指定する
if(IsMSVC)
  if(${CMAKE_GENERATOR_PLATFORM} STREQUAL x64)
    set(PLATFORM "x64")
    set(Isx64 "1")
  else()
    set(PLATFORM "x86")
    set(Isx86 "1")
  endif()
elseif(IsXcode)
  set(Isx64 "1")
  set(PLATFORM "x64")
endif()

# check that the target_path should be excluded
# if the path is excluded, ${OUTPUT} will be TRUE
function(CHECK_EXCLUSION_STATUS TARGET_PATH EXCLUDE_PATTERNS OUTPUT)
  set(${OUTPUT} FALSE PARENT_SCOPE)
  foreach(PAT ${EXCLUDE_PATTERNS})
    if(TARGET_PATH MATCHES ${PAT})
      set("${OUTPUT}" TRUE PARENT_SCOPE)
      break()
    endif()
  endforeach()
endfunction()

# DIRSに指定されたディレクトリの中からEXTERNSIONに指定された拡張子のファイルを探索し、
# ${OUTPUT}に追加する。
# 各ファイルは、BASE_DIRからの相対パスでSOURCE_GROUPが設定される。
function(CREATE_SOURCE_LIST BASE_DIR SOURCE_DIRS EXTENSIONS EXCLUDE_PATTERNS OUTPUT)
#   message("SOURCE_DIRS: ${SOURCE_DIRS}")
  unset(TMP_LIST)
  foreach(DIR ${SOURCE_DIRS})
    set(PATTERNS "")
    foreach(EXT ${EXTENSIONS})
      list(APPEND PATTERNS "${DIR}/${EXT}")
    endforeach()

    file(GLOB_RECURSE FILES ${PATTERNS})

    # Define SourceGroup reflecting filesystem hierarchy.
    foreach(FILE_PATH ${FILES})
      check_exclusion_status(${FILE_PATH} "${EXCLUDE_PATTERNS}" IS_EXCLUDED)
      if(IS_EXCLUDED)
        message("Excluded: ${FILE_PATH}")
        continue()
      endif()

      get_filename_component(FILE_PATH "${FILE_PATH}" ABSOLUTE)
      get_filename_component(DIR "${FILE_PATH}" DIRECTORY)
      file(RELATIVE_PATH GROUP_NAME "${BASE_DIR}" "${DIR}")
      string(REPLACE "/" "\\" GROUP_NAME "${GROUP_NAME}")
      source_group("${GROUP_NAME}" FILES "${FILE_PATH}")
      list(APPEND TMP_LIST "${FILE_PATH}")
    endforeach()
  endforeach()

#   message("SOURCES for ${BASE_DIR}: ${TMP_LIST}")

  set(${OUTPUT} ${TMP_LIST} PARENT_SCOPE)
endfunction()

function(GET_WXWIDGETS_LIBRARIES WX_LIB_DIR BUILD_TYPE OUTPUT)
  if(not IsMSVC)
    message(FATAL_ERROR "This function is only useful for Windows.")
  endif()

  string(TOLOWER "${BUILD_TYPE}" LCONF)

  if("${LCONF}" STREQUAL "debug")
    set(DP "d") # debug postfix
  else()
    set(DP "")
  endif()

  set(${OUTPUT}
    "${WX_LIB_DIR}/wxbase31u${DP}.lib" "${WX_LIB_DIR}/wxbase31u${DP}_net.lib"
    "${WX_LIB_DIR}/wxbase31u${DP}_xml.lib" "${WX_LIB_DIR}/wxmsw31u${DP}_core.lib"
    "${WX_LIB_DIR}/wxmsw31u${DP}_xrc.lib" "${WX_LIB_DIR}/wxmsw31u${DP}_webview.lib"
    "${WX_LIB_DIR}/wxmsw31u${DP}_stc.lib" "${WX_LIB_DIR}/wxmsw31u${DP}_richtext.lib"
    "${WX_LIB_DIR}/wxmsw31u${DP}_ribbon.lib" "${WX_LIB_DIR}/wxmsw31u${DP}_qa.lib"
    "${WX_LIB_DIR}/wxmsw31u${DP}_propgrid.lib" "${WX_LIB_DIR}/wxmsw31u${DP}_media.lib"
    "${WX_LIB_DIR}/wxmsw31u${DP}_html.lib" "${WX_LIB_DIR}/wxmsw31u${DP}_gl.lib"
    "${WX_LIB_DIR}/wxmsw31u${DP}_aui.lib" "${WX_LIB_DIR}/wxmsw31u${DP}_adv.lib"
    "${WX_LIB_DIR}/wxjpeg${DP}.lib" "${WX_LIB_DIR}/wxpng${DP}.lib" "${WX_LIB_DIR}/wxtiff${DP}.lib"
    "${WX_LIB_DIR}/wxzlib${DP}.lib" "${WX_LIB_DIR}/wxregexu${DP}.lib"
    "${WX_LIB_DIR}/wxexpat${DP}.lib" "${WX_LIB_DIR}/wxscintilla${DP}.lib"
    PARENT_SCOPE
    )
endfunction()
