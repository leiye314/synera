# Stage the installed runtime into the ignored build tree, never into source control.
# This makes local execution and CTest independent of Qt Creator or shell PATH.
if(WIN32)
    add_custom_command(TARGET synera POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:synera> $<TARGET_FILE_DIR:synera>
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:synera>/platforms
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt6::QWindowsIntegrationPlugin> $<TARGET_FILE_DIR:synera>/platforms
        COMMAND_EXPAND_LISTS VERBATIM)
    if(MINGW)
        foreach(runtime IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
            execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=${runtime}
                OUTPUT_VARIABLE runtime_path OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
            if(NOT EXISTS "${runtime_path}")
                message(FATAL_ERROR "Cannot locate compiler runtime: ${runtime}")
            endif()
            add_custom_command(TARGET synera POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${runtime_path}" $<TARGET_FILE_DIR:synera>
                VERBATIM)
        endforeach()
    endif()
    if(BUILD_TESTING)
        # Serialize shared runtime staging before tests link into the same output directory.
        add_dependencies(synera_selftest synera)
        add_dependencies(synera_widget_tests synera)
        add_custom_command(TARGET synera_widget_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt6::Test> $<TARGET_FILE_DIR:synera_widget_tests>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin> $<TARGET_FILE_DIR:synera_widget_tests>/platforms
            VERBATIM)
    endif()
endif()
