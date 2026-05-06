# ═══════════════════════════════════════════════════════════════
#  src/CMakeLists.txt — 组件注册
# ═══════════════════════════════════════════════════════════════
#
#  GLOB_RECURSE 会自动扫描 src/ 下所有 .c / .cpp 文件
#  如果你后续新增文件，不需要修改这里
#
#  REQUIRES 声明依赖的 IDF 组件：
#    driver  — UART 驱动 (uart_driver_install 等)
#    esp_wifi — Wi-Fi API
#    esp_event — 事件系统
#    esp_netif — TCP/IP 网络接口
#    nvs_flash — NVS 存储
#    lwip     — lwIP 协议栈 (udp_new, pbuf 等)
#    esp_timer — 高精度定时器 (esp_timer_get_time)

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES driver esp_wifi esp_event esp_netif nvs_flash lwip esp_timer
)
