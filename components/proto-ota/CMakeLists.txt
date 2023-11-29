set(component_srcs)

idf_component_register(SRCS "proto-ota.pb-c.c" "${component_srcs}"
                       PRIV_REQUIRES protobuf-c
                       INCLUDE_DIRS ".")