include_directories(
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/common
	${PROJECT_SOURCE_DIR}/putils
	${PROJECT_SOURCE_DIR}/putils/mediator
)

aux_source_directory(. SRCS)

# set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_compile_options(/bigobj)

file(GLOB_RECURSE SCRIPTS
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.*)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SCRIPTS})

add_executable(example ${SRCS} ${SCRIPTS})
target_link_libraries(example PRIVATE lua putils kengine)

if (MSVC)
	set_target_properties(example PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()