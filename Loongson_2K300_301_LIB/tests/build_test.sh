#!/bin/bash
# 阶段3/4 单元测试：龙芯交叉编译 + 部署车板运行（不碰电机）
# 用法:
#   ./build_test.sh                  编译全部测试
#   ./build_test.sh 192.168.43.161   编译 + 传到车板依次运行
set -e
cd "$(dirname "$0")/.."

TOOLCHAIN=./tools/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.6/bin/loongarch64-linux-gnu-g++

echo "== 编译 test_delivery (协议层单测) =="
$TOOLCHAIN -std=c++11 -O1 -Wall -Iuser_app -Ithird_party \
    tests/test_delivery.cpp \
    user_app/delivery_protocol.cpp \
    user_app/delivery_store.cpp \
    -o /tmp/test_delivery

echo "== 编译 test_gateway (网关+内嵌mock服务器) =="
$TOOLCHAIN -std=c++11 -O1 -Wall -pthread -Iuser_app -Ithird_party \
    tests/test_gateway.cpp \
    user_app/car_gateway.cpp \
    user_app/delivery_protocol.cpp \
    -o /tmp/test_gateway

echo "== 编译 mock_server (板端模拟服务器: 交互/--smoke冒烟) =="
$TOOLCHAIN -std=c++11 -O1 -Wall -pthread -Iuser_app -Ithird_party \
    tests/mock_server.cpp \
    user_app/delivery_protocol.cpp \
    -o /tmp/mock_server

echo "== 编译完成 =="

if [ -n "$1" ]; then
    for t in test_delivery test_gateway; do
        echo "== 部署并运行: $t =="
        cat /tmp/$t | ssh root@"$1" \
            "cat > /home/root/$t && chmod +x /home/root/$t && /home/root/$t"
    done
    echo "== 部署: mock_server (不运行) =="
    cat /tmp/mock_server | ssh root@"$1" "cat > /home/root/mock_server && chmod +x /home/root/mock_server"
fi
