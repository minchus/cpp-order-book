install(
    TARGETS cpp-order-book_exe
    RUNTIME COMPONENT cpp-order-book_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
