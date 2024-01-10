set(component_srcs "lora.c")

idf_component_register(SRCS "${component_srcs}"
                       PRIV_REQUIRES driver esp_timer
                       INCLUDE_DIRS ".")
