#ifndef __ROUTE_UDP_SERVER_HPP
#define __ROUTE_UDP_SERVER_HPP

#include <stdint.h>

/*============================================================================
 *                              路线前端 UDP 配置
 *============================================================================
 * 电脑路线选择 Web 前端默认运行在电脑侧 localhost:3000。
 * 浏览器不直接访问小车 UDP；电脑本地后端负责把 JSON 指令转发到小车 8898。
 */
#define ROUTE_SERVER_PORT     8898
#define ROUTE_SERVER_BUF_LEN  512

bool Route_Server_Init(void);
void Route_Server_Poll(void);

#endif /* __ROUTE_UDP_SERVER_HPP */
