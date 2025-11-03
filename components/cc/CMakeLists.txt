set(incs )
set(srcs )

file(GLOB_RECURSE CC_SOURCES cc/*.c port/*.c)

list(APPEND srcs
    ${CC_SOURCES}
)

list(APPEND incs
    cc/include
    port/include
)


idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS ${incs}
                    REQUIRES nvs_flash driver vfs esp_wifi app_update esp_timer bt mqtt http_client rbuffer)
