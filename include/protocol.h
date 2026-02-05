#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <cstdint>
#include <string>
#include <vector>

// 协议常量
#define FRAME_HEADER 0x68  // 帧头
#define FRAME_TAIL 0x16    // 帧尾

// 控制码定义
#define CONTROL_CODE_UPLINK 0x41    // 上行控制码（机器人发送）
#define CONTROL_CODE_DOWNLINK 0x82  // 下行控制码（平台下发）

// 协议帧结构
struct ProtocolFrame {
  uint8_t header;             // 帧头 0x68
  uint8_t control_code;       // 控制码（上行0x41，下行0x82）
  uint16_t number;            // 编号（2字节，robot_number）
  uint8_t frame_count;        // 帧计数（1字节，每次发送累加）
  uint8_t length;             // 数据长度（1字节，数据域字节数）
  std::vector<uint8_t> data;  // 数据域（标识1字节+参数N字节）
  uint8_t checksum;           // 校验和（累加和）
  uint8_t tail;               // 帧尾 0x16

  ProtocolFrame()
      : header(FRAME_HEADER),
        control_code(0),
        number(0),
        frame_count(0),
        length(0),
        checksum(0),
        tail(FRAME_TAIL) {}
};

class Protocol {
 public:
  Protocol();
  ~Protocol();

  // 编码：将数据打包成协议帧
  std::vector<uint8_t> Encode(uint8_t control_code, uint16_t number,
                              uint8_t frame_count,
                              const std::vector<uint8_t>& data);

  // 解码：解析协议帧
  bool Decode(const std::vector<uint8_t>& raw_data, ProtocolFrame& frame);

  // 计算校验和（累加和）
  static uint8_t CalculateChecksum(const std::vector<uint8_t>& data);

  // 验证校验和
  static bool VerifyChecksum(const std::vector<uint8_t>& raw_data);

  // 将字节数组转换为十六进制字符串
  static std::string BytesToHexString(const std::vector<uint8_t>& bytes);

  // 将十六进制字符串转换为字节数组
  static std::vector<uint8_t> HexStringToBytes(const std::string& hex_str);

  // 将字节数组转换为Base64编码字符串
  static std::string BytesToBase64(const std::vector<uint8_t>& bytes);

  // 将Base64编码字符串转换为字节数组
  static std::vector<uint8_t> Base64ToBytes(const std::string& base64_str);

 private:
  // 计算帧的校验和（不包括帧头、校验和、帧尾）
  uint8_t CalculateFrameChecksum(const ProtocolFrame& frame);
};

#endif  // PROTOCOL_H_
