/**
 * @file delivery_protocol.hpp
 * @brief 中央配送协议——JSON解析/序列化/NDJSON分帧/去重（纯逻辑，不依赖工程头文件）
 */

#ifndef __DELIVERY_PROTOCOL_HPP
#define __DELIVERY_PROTOCOL_HPP

#include "delivery_types.hpp"
#include <string>
#include <vector>

// nlohmann JSON（third_party，交叉编译已验证GCC8.3/C++11）
#include "nlohmann/json.hpp"

//================================================================================
// 解析结果
//================================================================================
struct ParseResult {
    bool            ok;
    DeliveryErrorCode code;
    std::string     message;

    ParseResult() : ok(true), code(ERR_NONE) {}
    static ParseResult Fail(DeliveryErrorCode c, const std::string& msg) {
        ParseResult r; r.ok = false; r.code = c; r.message = msg; return r;
    }
};

//================================================================================
// NDJSON 分帧器
// @details 处理TCP半包/粘包：累积字节流，按'\n'切分，去除'\r'，跳过空行；
//          单行超过 kMaxLineBytes 返回false（调用方应断开重连）
//================================================================================
class NdjsonDecoder {
public:
    bool Feed(const char* data, size_t len, std::vector<std::string>* out_lines);
    void Reset() { buf_.clear(); }
    size_t buffered() const { return buf_.size(); }

private:
    std::string buf_;
};

//================================================================================
// 工具函数
//================================================================================
// 当前UTC时间，格式 2026-08-27T12:34:56.789Z（sent_at用；本地超时判断禁用此值）
std::string Delivery_Format_Utc_Now();
// 消息ID：prefix + "-" + seq（调用方保证boot内seq递增即全局唯一）
std::string Delivery_Make_Message_Id(const std::string& prefix, uint64_t seq);
// FNV-1a 64位哈希（命令内容哈希用，非加密）
uint64_t Delivery_Fnv1a64(const std::string& s);
// 运动命令规范化内容哈希：只取业务字段按固定序序列化后哈希。
// message_id/sent_at等传输字段不参与，保证服务器重发同内容命令时哈希稳定。
uint64_t Delivery_Canonical_Command_Hash(const GotoStopCommand& cmd);

//================================================================================
// 服务器消息解析（一行JSON字符串 -> ServerMessage）
// @details 内含公共头校验（protocol_version==1, vehicle_id==0, 长度限制）
//================================================================================
ParseResult Delivery_Parse_Server_Message(const std::string& line, ServerMessage* out);

//================================================================================
// 小车消息序列化（返回JSON字符串，不含'\n'，由发送方追加）
//================================================================================
std::string Delivery_Serialize_Hello(const Hello& m, const CommonHeader& h);
std::string Delivery_Serialize_Heartbeat(const Heartbeat& m, const CommonHeader& h);
std::string Delivery_Serialize_SyncAck(const SyncAck& m, const CommonHeader& h);
std::string Delivery_Serialize_CommandAck(const CommandAck& m, const CommonHeader& h);
std::string Delivery_Serialize_NavigationStatus(const NavigationStatusReport& m, const CommonHeader& h);
std::string Delivery_Serialize_Arrived(const ArrivedEvent& m, const CommonHeader& h);
std::string Delivery_Serialize_UserAction(const UserActionEvent& m, const CommonHeader& h);
std::string Delivery_Serialize_Fault(const FaultReport& m, const CommonHeader& h);

//================================================================================
// 业务校验（纯函数）
//================================================================================
// 容量校验：status=3/4 的订单数不得超过 kMaxLoadedOrders
ParseResult Delivery_Check_Capacity(const std::vector<OrderInfo>& orders);
// 单订单校验：status必须1..5（禁止状态0假订单）、字符串长度限制
ParseResult Delivery_Check_Order_Valid(const OrderInfo& o);

#endif /* __DELIVERY_PROTOCOL_HPP */
