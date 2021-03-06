cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

set(SHORT_NAME openttd)
project(${SHORT_NAME})
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "OpenTTD")
set(VITA_TITLEID  "ANGU00001")

# include_directories(
  
# )

# add_executable(${SHORT_NAME}
  
# )

# target_link_libraries(${SHORT_NAME}
#   SDL2
#   vita2d
#   SceDisplay_stub
#   SceCtrl_stub
#   SceAudio_stub
#   SceSysmodule_stub
#   SceGxm_stub
#   SceCommonDialog_stub
#   m
# )

#configure_file(../objs/debug/${SHORT_NAME} ./${SHORT_NAME} COPYONLY)
configure_file(../objs/release/${SHORT_NAME} ./${SHORT_NAME} COPYONLY)

vita_create_self(${SHORT_NAME}.self ${SHORT_NAME})
vita_create_vpk(${SHORT_NAME}.vpk ${VITA_TITLEID} ${SHORT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)

add_custom_target(copyout
	COMMAND cp ./${SHORT_NAME}.vpk /mnt/c/tmp/${SHORT_NAME}.vpk
)

add_custom_target(send
	COMMAND unzip -o ./${SHORT_NAME}.vpk
	COMMAND curl -T eboot.bin ftp://192.168.144.54:1337/ux0:/app/${VITA_TITLEID}/
	DEPENDS ${SHORT_NAME}.vpk
)



