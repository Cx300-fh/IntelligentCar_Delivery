/**
 * @file test_delivery.cpp
 * @brief 阶段3纯协议层单元测试（交叉编译后在车板上运行，不碰电机）
 * @details 覆盖：NDJSON分帧/公共头校验/消息解析序列化/容量校验/两层去重/原子存储
 */

#include "delivery_protocol.hpp"
#include "delivery_store.hpp"

#include <cstdio>
#include <string>
#include <vector>
#include <fstream>

static int g_total = 0, g_fail = 0;

#define CHECK(cond) do { \
    ++g_total; \
    if (!(cond)) { ++g_fail; printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define SECTION(name) printf("== %s ==\n", name)

//================================================================================
// 1. NDJSON 分帧
//================================================================================
static void Test_Ndjson()
{
    SECTION("NDJSON分帧");
    NdjsonDecoder dec;
    std::vector<std::string> lines;

    // 半包
    CHECK(dec.Feed("{\"a\":", 5, &lines) == true);
    CHECK(lines.size() == 0);
    // 补齐一条
    char rest[] = "1}\n";
    CHECK(dec.Feed(rest, 3, &lines) == true);
    CHECK(lines.size() == 1);
    CHECK(lines[0] == "{\"a\":1}");
    lines.clear();

    // 粘包：一次两条 + 半条
    char two[] = "{\"b\":2}\n{\"c\":3}\n{\"d\":4";
    CHECK(dec.Feed(two, sizeof(two) - 1, &lines) == true);
    CHECK(lines.size() == 2);
    CHECK(lines[0] == "{\"b\":2}");
    CHECK(lines[1] == "{\"c\":3}");
    CHECK(dec.buffered() == 7);
    lines.clear();

    // \r\n 去除 + 空行跳过
    char crlf[] = "\r\n{\"e\":5}\r\n\n";
    CHECK(dec.Feed(crlf, sizeof(crlf) - 1, &lines) == true);
    CHECK(lines.size() == 1);
    CHECK(lines[0] == "{\"e\":5}");
    lines.clear();

    // 超限：单行超过64KiB必须拒绝
    std::string big(kMaxLineBytes + 2, 'x');
    CHECK(dec.Feed(big.data(), big.size(), &lines) == false);
    dec.Reset();

    // 中文UTF-8内容完整保留
    char cn[] = "{\"name\":\"\xe5\xb0\x8f\xe6\x98\x8e\"}\n";
    CHECK(dec.Feed(cn, sizeof(cn) - 1, &lines) == true);
    CHECK(lines.size() == 1);
    CHECK(lines[0].find("\xe5\xb0\x8f\xe6\x98\x8e") != std::string::npos);
}

//================================================================================
// 2. 工具函数
//================================================================================
static void Test_Utils()
{
    SECTION("工具函数");
    std::string t = Delivery_Format_Utc_Now();
    CHECK(t.size() == 24);
    CHECK(t[4] == '-' && t[10] == 'T' && t[t.size() - 1] == 'Z');

    CHECK(Delivery_Fnv1a64("abc") == Delivery_Fnv1a64("abc"));
    CHECK(Delivery_Fnv1a64("abc") != Delivery_Fnv1a64("abd"));

    std::string id = Delivery_Make_Message_Id("car-hb", 101);
    CHECK(id == "car-hb-101");
    CHECK(id.size() <= kMaxMessageIdLen);
}

//================================================================================
// 3. 服务器消息解析
//================================================================================
static const char* kHelloAckSample =
    "{\"protocol_version\":1,\"type\":\"hello_ack\",\"message_id\":\"srv-hello-1\","
    "\"vehicle_id\":0,\"sent_at\":\"2026-08-27T06:20:31.100Z\",\"accepted\":true,"
    "\"session_id\":\"session-001\",\"server_epoch\":\"server-epoch-x\","
    "\"heartbeat_interval_ms\":2000,\"connection_timeout_ms\":6000,"
    "\"latest_snapshot_version\":28,\"latest_command_version\":16,"
    "\"server_time\":\"2026-08-27T06:20:31.100Z\",\"error\":null}";

static const char* kStateSyncSample =
    "{\"protocol_version\":1,\"type\":\"state_sync\",\"message_id\":\"sync-29\","
    "\"vehicle_id\":0,\"sent_at\":\"2026-08-27T06:20:34.000Z\","
    "\"server_epoch\":\"server-epoch-x\",\"snapshot_version\":29,"
    "\"latest_command_version\":17,\"screen_phase\":3,"
    "\"current_order_id\":\"ORD-20260827-000045\",\"orders_total\":17,"
    "\"orders_included\":12,\"orders_truncated\":true,"
    "\"orders\":[{\"order_id\":\"ORD-20260827-000045\",\"display_no\":\"045\","
    "\"nickname\":\"\xe5\xb0\x8f\xe6\x98\x8e\",\"status\":3,\"status_code\":\"DELIVERING\","
    "\"status_text\":\"\xe9\x85\x8d\xe9\x80\x81\xe4\xb8\xad\",\"dispatch_state\":\"TO_DROPOFF\","
    "\"order_version\":8,\"pickup_location_id\":\"1:5\",\"pickup_name\":\"\xe4\xb8\x9c\xe5\xa4\xa7\xe6\x93\x8d\xe5\x9c\xba\","
    "\"dropoff_location_id\":\"1:13\",\"dropoff_name\":\"\xe7\x85\xa7\xe6\xbe\x9c\xe9\x99\xa2\","
    "\"item_summary\":\"\xe6\x96\x87\xe4\xbb\xb6\xe8\xa2\x8b\",\"button_label\":null,"
    "\"button_action\":null,\"button_enabled\":false}],"
    "\"active_trip\":{\"trip_id\":\"trip-21\",\"current_stop_id\":\"stop-8\",\"loaded_count\":1},"
    "\"authoritative_target\":{\"trip_id\":\"trip-21\",\"stop_id\":\"stop-8\","
    "\"map_id\":1,\"target_node\":13,\"command_version\":17}}";

static const char* kGotoStopSample =
    "{\"protocol_version\":1,\"type\":\"goto_stop\",\"message_id\":\"cmd-18\","
    "\"vehicle_id\":0,\"sent_at\":\"2026-08-27T06:20:35.000Z\","
    "\"command_version\":18,\"trip_id\":\"trip-21\",\"stop_id\":\"stop-8\","
    "\"map_id\":1,\"required_map_version\":3,\"required_map_checksum\":\"sha256:abc\","
    "\"target_node\":13,\"location_id\":\"1:13\",\"location_name\":\"ZLY\","
    "\"stop_type\":\"DROPOFF\",\"operations\":["
    "{\"order_id\":\"ORD-20260827-000045\",\"action\":\"DROPOFF\",\"order_version\":8}]}";

static void Test_Parse_Server()
{
    SECTION("服务器消息解析");
    ServerMessage msg;
    ParseResult r;

    // hello_ack
    r = Delivery_Parse_Server_Message(kHelloAckSample, &msg);
    CHECK(r.ok);
    CHECK(msg.type == SRV_MSG_HELLO_ACK);
    CHECK(msg.hello_ack.accepted);
    CHECK(msg.hello_ack.heartbeat_interval_ms == 2000);
    CHECK(msg.hello_ack.connection_timeout_ms == 6000);
    CHECK(msg.hello_ack.latest_command_version == 16);
    CHECK(msg.hello_ack.server_epoch == "server-epoch-x");
    CHECK(!msg.hello_ack.error.has_error);

    // state_sync
    r = Delivery_Parse_Server_Message(kStateSyncSample, &msg);
    CHECK(r.ok);
    CHECK(msg.type == SRV_MSG_STATE_SYNC);
    CHECK(msg.state_sync.snapshot_version == 29);
    CHECK(msg.state_sync.screen_phase == 3);
    CHECK(msg.state_sync.has_current_order);
    CHECK(msg.state_sync.orders.size() == 1);
    CHECK(msg.state_sync.orders[0].status == 3);
    CHECK(msg.state_sync.orders[0].order_version == 8);
    CHECK(msg.state_sync.orders[0].dispatch_state == DISPATCH_TO_DROPOFF);
    CHECK(msg.state_sync.active_trip.present);
    CHECK(msg.state_sync.active_trip.loaded_count == 1);
    CHECK(msg.state_sync.authoritative_target.present);
    CHECK(msg.state_sync.authoritative_target.target_node == 13);
    CHECK(Delivery_Check_Capacity(msg.state_sync.orders).ok);

    // goto_stop
    r = Delivery_Parse_Server_Message(kGotoStopSample, &msg);
    CHECK(r.ok);
    CHECK(msg.type == SRV_MSG_GOTO_STOP);
    CHECK(msg.goto_stop.command_version == 18);
    CHECK(msg.goto_stop.target_node == 13);
    CHECK(msg.goto_stop.required_map_version == 3);
    CHECK(msg.goto_stop.stop_type == STOP_TYPE_DROPOFF);
    CHECK(msg.goto_stop.operations.size() == 1);
    CHECK(msg.goto_stop.operations[0].order_id == "ORD-20260827-000045");
    CHECK(msg.goto_stop.operations[0].order_version == 8);

    // event_ack
    const char* event_ack =
        "{\"protocol_version\":1,\"type\":\"event_ack\",\"message_id\":\"srv-event-45\","
        "\"vehicle_id\":0,\"sent_at\":\"x\",\"reply_to\":\"evt-confirm-45\","
        "\"accepted\":true,\"event_type\":\"user_action\","
        "\"order_id\":\"ORD-20260827-000045\",\"new_status\":5,\"new_order_version\":9,"
        "\"snapshot_version\":30,\"error\":null}";
    r = Delivery_Parse_Server_Message(event_ack, &msg);
    CHECK(r.ok);
    CHECK(msg.type == SRV_MSG_EVENT_ACK);
    CHECK(msg.event_ack.accepted);
    CHECK(msg.event_ack.has_new_status && msg.event_ack.new_status == 5);
    CHECK(msg.event_ack.has_new_order_version && msg.event_ack.new_order_version == 9);

    // 空系统快照：phase=0 + null + 空订单
    const char* empty_sync =
        "{\"protocol_version\":1,\"type\":\"state_sync\",\"message_id\":\"sync-1\","
        "\"vehicle_id\":0,\"server_epoch\":\"e1\",\"snapshot_version\":1,"
        "\"screen_phase\":0,\"current_order_id\":null,\"orders\":[],"
        "\"active_trip\":null,\"authoritative_target\":null}";
    r = Delivery_Parse_Server_Message(empty_sync, &msg);
    CHECK(r.ok);
    CHECK(msg.state_sync.screen_phase == 0);
    CHECK(!msg.state_sync.has_current_order);
    CHECK(msg.state_sync.orders.empty());
    CHECK(!msg.state_sync.active_trip.present);
    CHECK(!msg.state_sync.authoritative_target.present);
}

//================================================================================
// 4. 非法输入拒绝
//================================================================================
static void Test_Rejections()
{
    SECTION("非法输入拒绝");
    ServerMessage msg;
    ParseResult r;

    // 非法JSON
    r = Delivery_Parse_Server_Message("{\"type\":", &msg);
    CHECK(!r.ok && r.code == ERR_INVALID_PROTOCOL);

    // protocol_version错误
    std::string s = kHelloAckSample;
    s.replace(s.find("\"protocol_version\":1"), 19, "\"protocol_version\":2");
    r = Delivery_Parse_Server_Message(s, &msg);
    CHECK(!r.ok && r.code == ERR_INVALID_PROTOCOL);

    // vehicle_id错误
    s = kHelloAckSample;
    s.replace(s.find("\"vehicle_id\":0"), 14, "\"vehicle_id\":3");
    r = Delivery_Parse_Server_Message(s, &msg);
    CHECK(!r.ok && r.code == ERR_WRONG_VEHICLE);

    // message_id超长
    std::string long_id(70, 'm');
    s = "{\"protocol_version\":1,\"type\":\"hello_ack\",\"message_id\":\"" + long_id +
        "\",\"vehicle_id\":0,\"accepted\":true}";
    r = Delivery_Parse_Server_Message(s, &msg);
    CHECK(!r.ok && r.code == ERR_INVALID_PROTOCOL);

    // 未知type
    r = Delivery_Parse_Server_Message(
        "{\"protocol_version\":1,\"type\":\"no_such_msg\",\"message_id\":\"x\",\"vehicle_id\":0}",
        &msg);
    CHECK(!r.ok && r.code == ERR_INVALID_PROTOCOL);

    // 状态0假订单必须拒绝
    s = "{\"protocol_version\":1,\"type\":\"state_sync\",\"message_id\":\"s\",\"vehicle_id\":0,"
        "\"server_epoch\":\"e\",\"snapshot_version\":2,\"screen_phase\":0,"
        "\"orders\":[{\"order_id\":\"ORD-1\",\"status\":0}]}";
    r = Delivery_Parse_Server_Message(s, &msg);
    CHECK(!r.ok && r.code == ERR_INVALID_PROTOCOL);

    // 容量超限：6个status=3
    std::string orders = "[";
    for (int i = 0; i < 6; i++) {
        if (i) orders += ",";
        orders += "{\"order_id\":\"ORD-" + std::to_string(i) + "\",\"status\":3}";
    }
    orders += "]";
    s = "{\"protocol_version\":1,\"type\":\"state_sync\",\"message_id\":\"s\",\"vehicle_id\":0,"
        "\"server_epoch\":\"e\",\"snapshot_version\":3,\"screen_phase\":3,\"orders\":" + orders + "}";
    r = Delivery_Parse_Server_Message(s, &msg);
    CHECK(r.ok);  // 解析本身允许
    ParseResult cap = Delivery_Check_Capacity(msg.state_sync.orders);
    CHECK(!cap.ok && cap.code == ERR_INVALID_CAPACITY_STATE);  // 业务校验拒绝

    // 订单数超过缓存上限
    std::string many = "[";
    for (int i = 0; i < 21; i++) {
        if (i) many += ",";
        many += "{\"order_id\":\"ORD-" + std::to_string(i) + "\",\"status\":1}";
    }
    many += "]";
    s = "{\"protocol_version\":1,\"type\":\"state_sync\",\"message_id\":\"s\",\"vehicle_id\":0,"
        "\"server_epoch\":\"e\",\"snapshot_version\":4,\"screen_phase\":1,\"orders\":" + many + "}";
    r = Delivery_Parse_Server_Message(s, &msg);
    CHECK(!r.ok && r.code == ERR_INVALID_CAPACITY_STATE);

    // goto_stop缺command_version
    s = kGotoStopSample;
    s.erase(s.find("\"command_version\":18,"), 21);
    r = Delivery_Parse_Server_Message(s, &msg);
    CHECK(!r.ok && r.code == ERR_INVALID_PROTOCOL);
}

//================================================================================
// 5. 规范化命令哈希 + 两层去重
//================================================================================
static void Test_Command_Dedup()
{
    SECTION("规范化哈希与两层去重");
    ServerMessage msg;
    ParseResult r = Delivery_Parse_Server_Message(kGotoStopSample, &msg);
    CHECK(r.ok);
    GotoStopCommand cmd_a = msg.goto_stop;

    // 相同业务内容、不同message_id -> 哈希必须相同（服务器重发场景）
    std::string s = kGotoStopSample;
    s.replace(s.find("cmd-18"), 6, "cmd-99");
    r = Delivery_Parse_Server_Message(s, &msg);
    CHECK(r.ok);
    GotoStopCommand cmd_b = msg.goto_stop;
    CHECK(Delivery_Canonical_Command_Hash(cmd_a) == Delivery_Canonical_Command_Hash(cmd_b));

    // 业务字段不同 -> 哈希不同
    GotoStopCommand cmd_c = cmd_a;
    cmd_c.target_node = 14;
    CHECK(Delivery_Canonical_Command_Hash(cmd_a) != Delivery_Canonical_Command_Hash(cmd_c));
    GotoStopCommand cmd_d = cmd_a;
    cmd_d.command_version = 19;  // 版本号参与哈希
    CHECK(Delivery_Canonical_Command_Hash(cmd_a) != Delivery_Canonical_Command_Hash(cmd_d));

    // 版本守卫四分支
    CommandVersionGuard guard;
    uint64_t ha = Delivery_Canonical_Command_Hash(cmd_a);
    uint64_t hc = Delivery_Canonical_Command_Hash(cmd_c);
    CHECK(guard.Check(17, ha) == CommandVersionGuard::ACCEPT_NEW);  // 首个命令
    guard.Record(17, ha, "{\"accepted\":true}");
    CHECK(guard.Check(16, ha) == CommandVersionGuard::STALE);
    CHECK(guard.Check(17, ha) == CommandVersionGuard::REPLAY_SAME);
    CHECK(guard.Check(17, hc) == CommandVersionGuard::CONFLICT);
    CHECK(guard.Check(18, hc) == CommandVersionGuard::ACCEPT_NEW);
    CHECK(guard.cached_ack() == "{\"accepted\":true}");  // 幂等重放返回缓存ACK
}

//================================================================================
// 6. 小车消息序列化
//================================================================================
static void Test_Serialize()
{
    SECTION("小车消息序列化");

    CommonHeader h;
    h.type = "heartbeat";
    h.message_id = "car-hb-101";
    h.sent_at = "2026-08-27T06:20:33.000Z";

    Heartbeat hb;
    hb.boot_id = "car-boot-a1";
    hb.sequence = 101;
    hb.current_node = 3;
    hb.has_trip = true; hb.trip_id = "trip-21";
    hb.has_stop = true; hb.stop_id = "stop-8";
    hb.command_version = 17;
    hb.position_source = POSITION_SOURCE_APRILTAG;
    hb.motion_state = MOTION_MOVING;

    std::string out = Delivery_Serialize_Heartbeat(hb, h);
    CHECK(out.find("\"sequence\":101") != std::string::npos);
    CHECK(out.find("\"trip_id\":\"trip-21\"") != std::string::npos);
    CHECK(out.find("\"stop_id\":\"stop-8\"") != std::string::npos);
    CHECK(out.find("\"position_source\":\"APRILTAG\"") != std::string::npos);
    CHECK(out.find("\"motion_state\":\"MOVING\"") != std::string::npos);
    CHECK(out.find("\"battery_percent\":null") != std::string::npos);

    // hello
    Hello hello;
    hello.device_token = "secret-token-do-not-log";
    hello.boot_id = "car-boot-a1";
    hello.last_command_version = 16;
    hello.current_node = 3;
    hello.position_valid = true;
    h.type = "hello";
    h.message_id = "car-boot-a1-hello";
    out = Delivery_Serialize_Hello(hello, h);
    CHECK(out.find("\"device_token\":\"secret-token-do-not-log\"") != std::string::npos);
    CHECK(out.find("\"last_command_version\":16") != std::string::npos);
    CHECK(out.find("\"emergency_latched\":false") != std::string::npos);

    // user_action
    UserActionEvent ua;
    ua.trip_id = "trip-21"; ua.stop_id = "stop-8";
    ua.order_id = "ORD-20260827-000045";
    ua.action = USER_ACTION_CONFIRM_DROPOFF_TAKEN;
    ua.expected_status = 4;
    ua.order_version = 8;
    ua.display_revision = 29;
    h.type = "user_action";
    h.message_id = "evt-confirm-45";
    out = Delivery_Serialize_UserAction(ua, h);
    CHECK(out.find("\"action\":\"CONFIRM_DROPOFF_TAKEN\"") != std::string::npos);
    CHECK(out.find("\"expected_status\":4") != std::string::npos);
    CHECK(out.find("\"display_revision\":29") != std::string::npos);

    // command_ack 失败示例
    CommandAck ack;
    ack.reply_to = "cmd-18";
    ack.accepted = false;
    ack.execution_state = "STOPPED";
    ack.error.has_error = true;
    ack.error.code = ERR_UNREACHABLE_TARGET;
    ack.error.message = "target node is unreachable";
    h.type = "command_ack";
    h.message_id = "ack-cmd-18";
    out = Delivery_Serialize_CommandAck(ack, h);
    CHECK(out.find("\"accepted\":false") != std::string::npos);
    CHECK(out.find("UNREACHABLE_TARGET") != std::string::npos);
}

//================================================================================
// 7. 本地可靠存储
//================================================================================
static void Test_Store()
{
    SECTION("本地可靠存储");
    const std::string path = "/tmp/test_delivery_store.json";

    DeliveryStoreData d;
    d.server_epoch = "server-epoch-x";
    d.snapshot_version = 29;
    d.command_version = 18;
    d.command_hash = 0x1234567890ABCDEFULL;
    d.command_json = "{\"type\":\"goto_stop\"}";
    d.emergency_latched = false;
    d.last_current_node = 13;
    d.map_id = 1; d.map_version = 3;
    d.map_checksum = "sha256:abc";
    PendingEvent pe;
    pe.message_id = "evt-arrive-8";
    pe.event_type = "arrived";
    pe.payload_json = "{\"type\":\"arrived\",\"message_id\":\"evt-arrive-8\"}";
    pe.created_at = "2026-08-27T06:25:00.000Z";
    d.pending_events.push_back(pe);

    CHECK(Delivery_Store_Save(path, d));

    DeliveryStoreData loaded;
    CHECK(Delivery_Store_Load(path, &loaded));
    CHECK(loaded.server_epoch == "server-epoch-x");
    CHECK(loaded.snapshot_version == 29);
    CHECK(loaded.command_version == 18);
    CHECK(loaded.command_hash == 0x1234567890ABCDEFULL);
    CHECK(loaded.command_json == "{\"type\":\"goto_stop\"}");
    CHECK(loaded.last_current_node == 13);
    CHECK(loaded.pending_events.size() == 1);
    CHECK(loaded.pending_events[0].message_id == "evt-arrive-8");
    CHECK(loaded.pending_events[0].payload_json.find("arrived") != std::string::npos);

    // 损坏文件：写一半的内容必须回退默认值且不崩溃
    { std::fstream f(path, std::ios::out | std::ios::trunc); f << "{\"server_epoch\":\"trun"; }
    CHECK(!Delivery_Store_Load(path, &loaded));
    CHECK(loaded.snapshot_version == 0);
    CHECK(loaded.pending_events.empty());

    // 文件不存在
    CHECK(!Delivery_Store_Load("/tmp/no_such_delivery_store.json", &loaded));
    CHECK(loaded.command_version == 0);

    remove(path.c_str());
}

//================================================================================
// main
//================================================================================
int main()
{
    printf("===== delivery 阶段3单元测试 =====\n");
    Test_Ndjson();
    Test_Utils();
    Test_Parse_Server();
    Test_Rejections();
    Test_Command_Dedup();
    Test_Serialize();
    Test_Store();
    printf("===== 结果: %d/%d 通过 =====\n", g_total - g_fail, g_total);
    return g_fail == 0 ? 0 : 1;
}
