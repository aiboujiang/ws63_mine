if(DEFINED CONFIG_SAMPLE_SUPPORT_MQTT_REMOTE_CAR)
    add_subdirectory_if_exist(mqtt_remote_car)
endif()


set(SOURCES "${SOURCES}" PARENT_SCOPE)