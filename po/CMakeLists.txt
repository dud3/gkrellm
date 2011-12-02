if (GETTEXT_FOUND)
    include ("${gkrellm_SOURCE_DIR}/cmake/gettext.cmake")
    gettext ("${gkrellm_SOURCE_DIR}/po" "gkrellm")
endif()
