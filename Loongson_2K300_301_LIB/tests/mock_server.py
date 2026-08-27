#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
中央配送模拟服务器（阶段4/5联调用，在虚拟机上运行）

用法:  python3 mock_server.py [端口，默认8898]

自动行为:
  hello      -> hello_ack (accepted=true)
  heartbeat  -> heartbeat_ack

键盘命令:
  sync            发送空 state_sync (screen_phase=0)
  goto [node]     发送 goto_stop (target_node默认13, command_version自增)
  hold            发送 hold
  estop           发送 emergency_stop
  resume          发送 resume
  silent          停止自动回复(模拟服务器无响应, 触发车端掉线判定)
  alive           恢复自动回复
  drop            主动断开当前连接
  quit            退出
"""
import socket
import threading
import time
import sys
import json

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8898

state = {
    "silent": False,
    "cmd_ver": 17,
    "snap_ver": 1,
}

lock = threading.Lock()
client_sock = None   # 当前客户端


def now_iso():
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime()) + ".000Z"


def send_obj(sock, obj):
    try:
        sock.sendall((json.dumps(obj, ensure_ascii=False) + "\n").encode("utf-8"))
    except OSError:
        pass


def common(t):
    return {
        "protocol_version": 1,
        "type": t,
        "message_id": "srv-%s-%d" % (t, int(time.time() * 1000) % 100000),
        "vehicle_id": 0,
        "sent_at": now_iso(),
    }


def send_sync(sock):
    global state
    with lock:
        state["snap_ver"] += 1
        sv = state["snap_ver"]
    m = common("state_sync")
    m.update({
        "server_epoch": "mock-epoch-1",
        "snapshot_version": sv,
        "latest_command_version": state["cmd_ver"],
        "screen_phase": 0,
        "current_order_id": None,
        "orders": [],
        "active_trip": None,
        "authoritative_target": None,
    })
    send_obj(sock, m)
    print(">> state_sync v%d" % sv)


def send_goto(sock, node):
    global state
    with lock:
        state["cmd_ver"] += 1
        cv = state["cmd_ver"]
    m = common("goto_stop")
    m.update({
        "command_version": cv,
        "trip_id": "mock-trip-1",
        "stop_id": "mock-stop-1",
        "map_id": 1,
        "required_map_version": 3,
        "required_map_checksum": "sha256:mock",
        "target_node": node,
        "location_id": "1:%d" % node,
        "location_name": "MOCK节点%d" % node,
        "stop_type": "DROPOFF",
        "operations": [{"order_id": "ORD-MOCK-1", "action": "DROPOFF",
                        "order_version": 1}],
    })
    send_obj(sock, m)
    print(">> goto_stop v%d -> node %d" % (cv, node))


def handle_line(sock, line):
    try:
        m = json.loads(line)
    except ValueError:
        print("!! 非法JSON: %.80s" % line)
        return
    t = m.get("type", "?")
    print("<< %-18s %.100s" % (t, line))
    if state["silent"]:
        return
    if t == "hello":
        ack = common("hello_ack")
        ack.update({
            "accepted": True,
            "session_id": "mock-session-1",
            "server_epoch": "mock-epoch-1",
            "heartbeat_interval_ms": 2000,
            "connection_timeout_ms": 6000,
            "latest_snapshot_version": state["snap_ver"],
            "latest_command_version": state["cmd_ver"],
            "server_time": now_iso(),
            "error": None,
        })
        send_obj(sock, ack)
        print(">> hello_ack")
    elif t == "heartbeat":
        ack = common("heartbeat_ack")
        ack.update({
            "server_epoch": "mock-epoch-1",
            "snapshot_version": state["snap_ver"],
            "latest_command_version": state["cmd_ver"],
        })
        send_obj(sock, ack)


def client_thread(sock, addr):
    global client_sock
    client_sock = sock
    print("== 车端已连接: %s" % str(addr))
    buf = b""
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.rstrip(b"\r").decode("utf-8", "replace")
                if line:
                    handle_line(sock, line)
    except OSError:
        pass
    finally:
        print("== 车端断开")
        sock.close()
        if client_sock is sock:
            client_sock = None


def main():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", PORT))
    srv.listen(1)
    print("模拟服务器监听 0.0.0.0:%d （等待车端连接...）" % PORT)

    def accept_loop():
        srv.settimeout(0.5)
        while True:
            try:
                s, a = srv.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            threading.Thread(target=client_thread, args=(s, a), daemon=True).start()

    threading.Thread(target=accept_loop, daemon=True).start()

    while True:
        try:
            cmd = input("mock> ").strip()
        except EOFError:
            break
        if not cmd:
            continue
        if cmd == "quit":
            break
        elif cmd == "silent":
            state["silent"] = True
            print("（停止自动回复）")
        elif cmd == "alive":
            state["silent"] = False
            print("（恢复自动回复）")
        elif cmd == "drop":
            if client_sock:
                client_sock.close()
            else:
                print("（无连接）")
        elif cmd.startswith("sync"):
            if client_sock and not state["silent"]:
                send_sync(client_sock)
        elif cmd.startswith("goto"):
            parts = cmd.split()
            node = int(parts[1]) if len(parts) > 1 else 13
            if client_sock and not state["silent"]:
                send_goto(client_sock, node)
        elif cmd in ("hold", "estop", "resume"):
            if client_sock and not state["silent"]:
                with lock:
                    state["cmd_ver"] += 1
                    cv = state["cmd_ver"]
                m = common({"hold": "hold", "estop": "emergency_stop",
                            "resume": "resume"}[cmd])
                m.update({
                    "command_version": cv,
                    "trip_id": "mock-trip-1",
                    "stop_id": "mock-stop-1",
                    "reason": "mock测试",
                })
                if cmd == "resume":
                    m["resume_target_command_version"] = cv - 1
                send_obj(client_sock, m)
                print(">> %s v%d" % (cmd, cv))
        else:
            print("（未知命令: %s）" % cmd)
    srv.close()


if __name__ == "__main__":
    main()
