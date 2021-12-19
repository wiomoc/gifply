cmake_minimum_required(VERSION 3.4.1)

add_library(gifsicle
             SHARED
             lib.c
             src/gifwrite.c
             src/gifread.c
             src/giffunc.c
             src/optimize.c
             src/quantize.c
             src/support.c
             src/merge.c
             src/gifsicle.c
             src/gifunopt.c
             src/xform.c
             src/clp.c
)
find_library( log-lib log )
target_link_libraries(gifsicle ${log-lib} )
target_include_directories(gifsicle PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")
