cmake_minimum_required(VERSION 3.16)

project(Log_analyzer VERSION 0.1 LANGUAGES CXX)

# 多配置支持
if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;MinSizeRel;RelWithDebInfo")
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING 
        "Build configurations" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 构建类型特定的标志
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -flto")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g -DNDEBUG")

set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")

# 性能优化设置
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -flto")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置Qt策略
set(QT_USE_LEGACY_QML_RESOURCE_PREFIX FALSE)

# 启用资源压缩以减少包大小
set(CMAKE_AUTORCC_OPTIONS "--compress;9;--threshold;0")

set(SSHPASS_SOURCE_PATH "D:/MSYS2/usr/bin/sshpass.exe")
set(SSH_SOURCE_PATH "D:/MSYS2/usr/bin/ssh.exe")
set(SCP_SOURCE_PATH "D:/MSYS2/usr/bin/scp.exe")
set(SSH_TOOLS_DEPLOY_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin/ssh_tools")


# 添加 QML 语言服务器配置
set(QT_QML_GENERATE_QMLLS_INI ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Widgets Network Sql)

# set(ZLIB_ROOT "D:/MSYS2/mingw64") # 请确保这是你 MSYS2 mingw64 的实际根目录
# set(CMAKE_PREFIX_PATH "${ZLIB_ROOT};${CMAKE_PREFIX_PATH}")
# find_package(ZLIB REQUIRED)

set(VTA_SUBPROCESS_BASE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shared_lib")

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(appLog_analyzer
    src/main.cpp
    src/textfilehandler.cpp
    src/textfilehandler.h
    src/sqlite_text_handler.cpp
    src/sqlite_text_handler.h
    src/ssh_file_manager.cpp
    src/ssh_file_manager.h
    src/app_manager.cpp
    src/app_manager.h
    src/update_checker.cpp
    src/update_checker.h
    src/tcpclient.cpp
    src/tcpclient.h
    src/log_analyzer_subprocess.cpp
    src/log_analyzer_subprocess.h
    src/local_ipc_communication.cpp
    src/local_ipc_communication.h
    src/map_xml_parser.cpp
    src/map_xml_parser.h
    src/map_data_manager.cpp
    src/map_data_manager.h
)

target_include_directories(appLog_analyzer PRIVATE
    "${VTA_SUBPROCESS_BASE_ROOT_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
find_library(VTA_SUBPROCESS_BASE_LIB
    NAMES vta_subprocess_base libvta_subprocess_base # 查找的库名称
    PATHS "${VTA_SUBPROCESS_BASE_ROOT_DIR}/lib" # 库文件实际所在路径
    REQUIRED # 如果找不到则报错
)

qt6_add_qml_module(appLog_analyzer
    URI Log_analyzer
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/TextAnalyzerPage.qml
        qml/UpdateNotificationCard.qml
        qml/VehicleReviewPage.qml
        qml/MapViewer.qml
     RESOURCES
        qml/qml.qrc
)

qt_add_resources(appLog_analyzer "qml/qml.qrc"
    PREFIX "/"
    FILES
        qml/Main.qml
        qml/TextAnalyzerPage.qml
        qml/UpdateNotificationCard.qml
        qml/VehicleReviewPage.qml
        qml/MapViewer.qml
)


target_link_libraries(appLog_analyzer
    PRIVATE 
    ${VTA_SUBPROCESS_BASE_LIB}
    Qt6::Quick Qt6::Widgets Qt6::Network Qt6::Sql 
)



include(GNUInstallDirs)
install(TARGETS appLog_analyzer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)



# 添加 updater 子目录，使其在编译主项目时一同编译
# add_subdirectory(updater)

# Windows 特定设置
if(WIN32)
    add_custom_command(TARGET appLog_analyzer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SSH_TOOLS_DEPLOY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "${SSHPASS_SOURCE_PATH}" "${SSH_TOOLS_DEPLOY_DIR}/sshpass.exe"
    COMMAND ${CMAKE_COMMAND} -E copy "${SSH_SOURCE_PATH}" "${SSH_TOOLS_DEPLOY_DIR}/ssh.exe"
    COMMAND ${CMAKE_COMMAND} -E copy "${SCP_SOURCE_PATH}" "${SSH_TOOLS_DEPLOY_DIR}/scp.exe"
    COMMENT "Copying sshpass.exe for deployment"
    )

    set_target_properties(appLog_analyzer PROPERTIES
        WIN32_EXECUTABLE TRUE
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    # ---- 自动打包ssh.exe依赖 ----
    # 假定ssh.exe已被部署到${SSH_TOOLS_DEPLOY_DIR}
    set(USER_SSH_EXE_PATH "${SSH_TOOLS_DEPLOY_DIR}/ssh.exe")
    if(EXISTS "${USER_SSH_EXE_PATH}")
        # 临时目录保存dll路径列表
        set(SSH_DLL_LIST "${CMAKE_CURRENT_BINARY_DIR}/ssh_dlls.txt")
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES "${USER_SSH_EXE_PATH}"
            RESOLVED_DEPENDENCIES_VAR found_ssh_dlls
            UNRESOLVED_DEPENDENCIES_VAR missing_ssh_dlls
            # 忽略系统API集
            PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
            POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        )
        if(found_ssh_dlls)
            foreach(dll_path IN LISTS found_ssh_dlls)
                get_filename_component(dll_name "${dll_path}" NAME)
                add_custom_command(TARGET appLog_analyzer POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${dll_path}"
                        "${SSH_TOOLS_DEPLOY_DIR}/${dll_name}"
                    COMMENT "Auto copying ${dll_name} needed by ssh.exe"
                )
            endforeach()
        endif()
    else()
        message(WARNING "[SSH] ssh.exe未找到，无法自动收集依赖DLL。请先将ssh.exe手动放入${SSH_TOOLS_DEPLOY_DIR}，然后重新构建。")
    endif()
endif()
