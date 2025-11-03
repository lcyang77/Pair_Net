file(GLOB_RECURSE GS_SOURCES gs/*.c)

idf_component_register(SRCS "main.c" 
                            "product.c"
                            "frame_parser.c"
                            ${GS_SOURCES}
                    INCLUDE_DIRS "."
                            "gs/include")
