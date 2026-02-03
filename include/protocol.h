#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <cstdint>
#include <string>
#include <vector>

// 协议常量
#define FRAME_HEADER 0x68  // 帧头
#define FRAME_TAIL 0x16    // 帧尾

// 数据方向
enum class DataDirection : uint8_t {
  kUplink = 0x01,   // 上行
  kDownlink = 0x02  // 下行
};

// 协议帧结构
struct ProtocolFrame {
  uint8_t header;             // 帧头 0x68
  uint8_t control_code;       // 控制码
  uint8_t number;             // 编号
  uint8_t frame_count;        // 帧计数
  uint16_t length;            // 数据长度（数据域字节数）
  std::vector<uint8_t> data;  // 数据域（标识+参数）
  uint8_t checksum;           // 校验和
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
  std::vector<uint8_t> Encode(uint8_t control_code, uint8_t number,
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
