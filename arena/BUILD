cc_library(
    name = 'arena',
    hdrs = [
        'arena.h',
    ],  
    srcs = [
        'arena.cc',
    ],  
    deps = [
        '//arena:mempool',
    ],  
    optimize = [
        '-D__USING_STD__',
    ],
)

cc_library(
    name = 'mempool',
    hdrs = [
        'mmap_mempool.h',
    ],
    srcs = [
        'mmap_mempool.cc',
    ],
    deps = [
    ],
    optimize = [
        '-D__USING_STD__',
    ],
)

#cc_test(
#    name = 'arena_test',
#    srcs = [
#        'arena_test.cc',
#    ],
#    deps = [
#        '//arena:arena',
#    ],
#    optimize = [
#        '-D__USING_STD__',
#    ],
#)

