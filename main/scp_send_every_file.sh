#!/bin/bash

# ====================== 全局配置区（脚本基础设置） ======================
# 默认远程存放路径（未指定参数三时自动使用）
DEFAULT_REMOTE_PATH="/home/root/"
# 远程服务器 SSH 端口（默认22）
SSH_PORT=22
# 远程登录用户名（板卡默认 root）
REMOTE_USER="root"
# =====================================================================

# ====================== 打印帮助信息 ======================
# 作用：脚本无参数、-h、--help 时显示使用说明
show_help() {
cat << EOF
======================================== SCP 自动传输脚本 ========================================
脚本作用：通过 SCP 指令将本地任意文件/文件夹传输到远程服务器，并自动配置权限
使用方法：
    1. 基础用法（默认路径）：	$0 远程IP地址 本地文件/文件夹
    2. 自定义路径：           	$0 远程IP地址 本地文件/文件夹 远程存放路径
    3. 查看帮助：             	$0 -h  |  $0 --help

参数说明：
    参数1    必选    远程服务器IP地址（如 192.168.1.100）
    参数2    必选    本地要传输的文件/文件夹路径
    参数3    可选    远程服务器存放路径（默认：${DEFAULT_REMOTE_PATH}）

脚本能力：
    ✅ 支持传输单个文件、整个文件夹（递归传输）
    ✅ 自动给传输后的文件/文件夹添加 读写权限 (chmod 644)
    ✅ 自动判断可执行文件（sh、py、bin、bash、run等），自动添加 执行权限 (chmod +x)
    ✅ 自带参数校验、文件存在性校验、错误提示
    ✅ 显示传输进度与执行结果
===================================================================================================
EOF
}

# ====================== 参数校验与赋值 ======================
# 作用：检查用户输入参数是否合法，并给全局变量赋值
check_params() {
    # 无参数 或 输入帮助参数，显示帮助并退出
    if [ $# -eq 0 ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
        show_help
        exit 0
    fi

    # 参数数量必须是 2 个或 3 个
    if [ $# -lt 2 ] || [ $# -gt 3 ]; then
        echo -e "\033[31m错误：参数数量不正确！\033[0m"
        show_help
        exit 1
    fi

    # 赋值参数到全局变量
    REMOTE_IP="$1"        # 远程IP
    LOCAL_PATH="$2"       # 本地文件/文件夹
    REMOTE_PATH="${3:-$DEFAULT_REMOTE_PATH}"  # 远程路径（可选）

    # 校验本地文件是否存在
    if [ ! -e "${LOCAL_PATH}" ]; then
        echo -e "\033[31m错误：文件不存在 → ${LOCAL_PATH}\033[0m"
        exit 1
    fi
}

# ====================== 打印传输开始信息 ======================
# 作用：输出传输前的提示信息
print_start_info() {
    echo -e "\033[32m======= 开始传输 =======\033[0m"
    echo "目标IP：$REMOTE_IP"
    echo "本地文件：$LOCAL_PATH"
    echo "远程路径：$REMOTE_PATH"
    echo -e "------------------------------------------------\033[0m"
}

# ====================== 检查并创建远程目录 ======================
# 作用：如果远程目标文件夹不存在，自动创建
create_remote_directory() {
    ssh -o LogLevel=error -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "${REMOTE_USER}@${REMOTE_IP}" "mkdir -p '${REMOTE_PATH}'"
}

# ====================== 执行 SCP 传输 ======================
# 作用：判断是文件还是文件夹，执行对应传输命令（保留进度条 + 关闭警告）
do_scp_transfer() {
    if [ -d "${LOCAL_PATH}" ]; then
        # 文件夹：递归传输 -r
        scp -P ${SSH_PORT} \
            -O \
            -o LogLevel=error \
            -o StrictHostKeyChecking=no \
            -o UserKnownHostsFile=/dev/null \
            -r "${LOCAL_PATH}" "${REMOTE_USER}@${REMOTE_IP}:${REMOTE_PATH}"
    else
        # 文件：普通传输
        scp -P ${SSH_PORT} \
            -O \
            -o LogLevel=error \
            -o StrictHostKeyChecking=no \
            -o UserKnownHostsFile=/dev/null \
            "${LOCAL_PATH}" "${REMOTE_USER}@${REMOTE_IP}:${REMOTE_PATH}"
    fi

    # 判断传输是否失败
    if [ $? -ne 0 ]; then
        echo -e "\033[31m传输失败！\033[0m"
        exit 1
    fi
}

# ====================== 远程自动配置权限 ======================
# 作用：给文件/文件夹赋读写权限，可执行文件自动加执行权限
set_remote_permissions() {
    echo -e "\n\033[32m传输成功，正在自动配置权限...\033[0m"

    # 获取文件名，拼接远程完整路径
    FILE_NAME=$(basename "${LOCAL_PATH}")
    REMOTE_FULL="${REMOTE_PATH}/${FILE_NAME}"

    # SSH 远程执行权限命令（静默无警告）
    ssh -o LogLevel=error -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "${REMOTE_USER}@${REMOTE_IP}" "
    chmod -R 644 '${REMOTE_FULL}'
    if [ -d '${REMOTE_FULL}' ]; then
        find '${REMOTE_FULL}' -type f \( -name '*.sh' -o -name '*.py' -o -name '*.bin' -o -name '*.run' \) -exec chmod +x {} \; 2>/dev/null
    else
        if [[ \${REMOTE_FULL} =~ \.(sh|py|bin|run)$ ]]; then
            chmod +x '${REMOTE_FULL}';
        fi
    fi
    "
}

# ====================== 打印完成信息 ======================
# 作用：输出最终成功提示
print_finish_info() {
    echo -e "\033[32m------------------------------------------------"
    echo -e "===== ✅ 传输完成！=====\033[0m"
    echo "文件位置：root@${REMOTE_IP}:${REMOTE_FULL}"
}

# ====================== 主程序入口（按顺序调用函数） ======================
check_params "$@"    		# 校验参数
print_start_info     		# 打印开始信息
create_remote_directory     # 自动创建远程文件夹（新增）
do_scp_transfer      		# 执行传输
set_remote_permissions	 	# 设置权限
print_finish_info    		# 打印完成信息
exit 0               		# 正常退出

