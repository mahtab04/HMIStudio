# hmi_set_project_options(<target>)
#
# Applies the project's warning level, warnings-as-errors and sanitizer
# settings to one of our own targets. Third-party targets are left alone.
function(hmi_set_project_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        if(HMI_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        # -Wno-missing-field-initializers: descriptor tables use designated
        # initializers and omit fields on purpose; GCC 13 warns about that.
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow
                                                 -Wnon-virtual-dtor -Woverloaded-virtual
                                                 -Wno-missing-field-initializers)
        if(HMI_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(HMI_ENABLE_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined
                                                     -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()

    target_compile_definitions(${target} PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
        QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
        QT_NO_KEYWORDS
        # A release-only Qt kit under CMake >= 4.2 (policy CMP0199) defines
        # QT_NO_DEBUG even in our Debug builds, which would compile out
        # Q_ASSERT. Keep assertions on in Debug regardless of the Qt kit.
        $<$<CONFIG:Debug>:QT_FORCE_ASSERTS>)
endfunction()
