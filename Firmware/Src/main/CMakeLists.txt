file(GLOB_RECURSE APP_SOURCES "*.cpp" "*.hpp" "*.c")
idf_component_register(
    SRCS ${APP_SOURCES}
    INCLUDE_DIRS "."
)