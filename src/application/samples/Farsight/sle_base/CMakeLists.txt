if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_01_DEVICEDISCOVERY_GRANTNODE)
    add_subdirectory_if_exist(sle_base_01_DeviceDiscovery_GrantNode)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_01_DEVICEDISCOVERY_TERMINALNODE)
    add_subdirectory_if_exist(sle_base_01_DeviceDiscovery_TerminalNode)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_02_CONNECTIONMANAGER_GRANTNODE)
    add_subdirectory_if_exist(sle_base_02_ConnectionManager_GrantNode)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_02_CONNECTIONMANAGER_TERMINALNODE)
    add_subdirectory_if_exist(sle_base_02_ConnectionManager_TerminalNode)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_03_SSAP_SERVER)
    add_subdirectory_if_exist(sle_base_03_SSAP_Server)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_03_SSAP_CLIENT)
    add_subdirectory_if_exist(sle_base_03_SSAP_Client)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_04_SSAP_CLIENT_WRITEREAD)
    add_subdirectory_if_exist(sle_base_04_SSAP_Client_WriteRead)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_05_NOTIINDI_SERVER)
    add_subdirectory_if_exist(sle_base_05_NotiIndi_Server)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_05_NOTIINDI_CLIENT)
    add_subdirectory_if_exist(sle_base_05_NotiIndi_Client)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_06_DELAYTEST_CLIENT)
    add_subdirectory_if_exist(sle_base_06_DelayTest_Client)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_06_DELAYTEST_SERVER)
    add_subdirectory_if_exist(sle_base_06_DelayTest_Server)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_07_1VN_CLIENT)
    add_subdirectory_if_exist(sle_base_07_1vN_Client)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_08_HYBRID_CLIENTNODE)
    add_subdirectory_if_exist(sle_base_08_Hybrid_ClientNode)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_08_HYBRID_HYBRIDNODE)
    add_subdirectory_if_exist(sle_base_08_Hybrid_HybridNode)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_BASE_08_HYBRID_SERVERNODE)
    add_subdirectory_if_exist(sle_base_08_Hybrid_ServerNode)
endif()

set(SOURCES "${SOURCES}" PARENT_SCOPE)