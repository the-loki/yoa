macro(yoa_subdir result curdir)
    FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
    SET(dirlist "")
    foreach (child ${children})
        if (IS_DIRECTORY ${curdir}/${child})
            LIST(APPEND dirlist ${child})
        endif ()
    endforeach ()
    SET(${result} ${dirlist})
endmacro()