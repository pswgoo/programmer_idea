macro(add_source_groups)
	# Calculate needed groups
	foreach(FILE ${ARGV})
		if(${FILE} MATCHES "/")
			string(REGEX REPLACE "(.+)/.+" "\\1" GROUP ${FILE})
			list(APPEND GROUPS ${GROUP})
		endif()
	endforeach(FILE)
	if(GROUPS)
		list(REMOVE_DUPLICATES GROUPS)
		# Create each group with its files
		foreach(GROUP ${GROUPS})
			# Find files belonging to this group
			set(GROUP_FILES)
			foreach(FILE ${ARGV})
				if(${FILE} MATCHES "^${GROUP}/[^/]+$")
					list(APPEND GROUP_FILES ${FILE})
				endif()
			endforeach(FILE)
			string(REPLACE "/" "\\" GROUP ${GROUP})
			source_group("Source\\${GROUP}" FILES ${GROUP_FILES})
		endforeach(GROUP)
	endif(GROUPS)

	# Add top level group
	set(GROUP_FILES)
	foreach(FILE ${ARGV})
		if(NOT (${FILE} MATCHES "/"))
			list(APPEND GROUP_FILES ${FILE})
		endif()
	endforeach(FILE)
	source_group("Source" FILES ${GROUP_FILES})
endmacro(add_source_groups)

