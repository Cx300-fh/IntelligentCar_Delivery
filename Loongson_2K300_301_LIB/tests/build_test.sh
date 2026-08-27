#!/bin/bash
# 阶段3纯协议层单测：龙芯交叉编译 + 部署车板运行（不碰电机）
# 用法:
#   ./build_test.sh            仅交叉编译
#   ./build_test.sh 192.168.43.161   编译 + 传到车板运行
set -e
cd "$(dirname "$0")/.."

TOOLCHAIN=./tools/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.6/bin/loongarch64-linux-gnu-g++

$TOOLCHAIN -std=c++11 -O1 -Wall -Iuser_app -Ithird_party \
    tests/test_delivery.cpp \
    user_app/delivery_protocol.cpp \
    user_app/delivery_store.cpp \
    -o /tmp/test_delivery

echo "== 编译完成: /tmp/test_delivery (loongarch64) =="

if [ -n "$1" ]; then
    echo "== 部署到车板 $1 并运行 =="
    cat /tmp/test_delivery | ssh root@"$1" \
        "cat > /home/root/test_delivery && chmod +x /home/root/test_delivery && /home/root/test_delivery"
fi
