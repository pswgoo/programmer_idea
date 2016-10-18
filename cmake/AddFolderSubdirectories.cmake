macro(add_folder_subdirectories FOLDER)
	foreach(DIR ${ARGN})
		add_subdirectory(${DIR})
		set_target_properties(${DIR} PROPERTIES FOLDER ${FOLDER})
	endforeach(DIR)
endmacro()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER _CMakePredefinedTargets)

