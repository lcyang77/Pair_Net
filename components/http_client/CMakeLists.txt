idf_component_register(SRCS "http/src/http_client.c"
                            "http/src/http_formdata.c"
                            "http/src/http_method_api.c"
                            "http_wrapper.c"
                    INCLUDE_DIRS "http/include" "http/internal" 
                    REQUIRES lwip)