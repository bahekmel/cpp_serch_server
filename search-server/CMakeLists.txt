cmake_minimum_required(VERSION 3.20)
project(cpp-search-server)

set(CMAKE_CXX_STANDARD 17)

add_executable(FP_sprint_4 main.cpp document.cpp document.h paginator.h read_input_functions.cpp read_input_functions.h remove_duplicates.cpp remove_duplicates.h request_queue.cpp request_queue.h search_server.cpp search_server.h string_processing.cpp string_processing.h test_example_functions.cpp test_example_functions.h process_queries.cpp process_queries.h)
