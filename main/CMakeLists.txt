FILE(GLOB app_sources *.hpp *.cpp)

idf_component_register(SRCS ${app_sources}
                    INCLUDE_DIRS ".")
