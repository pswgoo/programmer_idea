function(set_project_warnings target)
  target_compile_options(${target} PRIVATE
    -Wall -Wextra -Wpedantic
    -Wshadow -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor -Wold-style-cast
    -Woverloaded-virtual -Wnull-dereference
    -Wdouble-promotion -Wformat=2
  )
endfunction()
