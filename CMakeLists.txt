CU_SET_PATH("CMAKE_EMBLEM_TRANSFER_DIR" "${CMAKE_CURRENT_LIST_DIR}")

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/world_emblem_transfer.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/npc_emblem_transfer.cpp")

AC_ADD_SCRIPT_LOADER("WorldEmblemTransfer" "${CMAKE_CURRENT_LIST_DIR}/src/loader.h")
AC_ADD_SCRIPT_LOADER("NpcEmblemTransfer" "${CMAKE_CURRENT_LIST_DIR}/src/loader.h")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/emblem_transfer.conf.dist")

CU_ADD_HOOK(AFTER_WORLDSERVER_CMAKE "${CMAKE_CURRENT_LIST_DIR}/src/cmake/after_ws_install.cmake")
