# Add the library target
add_library(axis_controller STATIC 
axle_controller.c
cJSON.c
common_utils.c
json_utils.c
logz.c
socket_utils.c
vlitem_handler.c)
# Link libraries
target_link_libraries(axis_controller PRIVATE wsock32 ws2_32)
# Include directories
target_include_directories(axis_controller PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)