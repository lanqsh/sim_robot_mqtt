#include "protocol.h"

#include <glog/logging.h>

#include <iomanip>
#include <sstream>

Protocol::Protocol() {}

Protocol::~Protocol() {}

std::vector<uint8_t> Protocol::Encode(uint8_t control_code, uint8_t number,
                                      uint8_t frame_count,
                                      const std::vector<uint8_t>& data) {
  ProtocolFrame frame;
  frame.control_code = control_code;
  frame.number = number;
  frame.frame_count = frame_count;
  frame.length = static_cast<uint16_t>(data.size());
  frame.data = data;

  // 构建完整帧（用于计算校验和）
  std::vector<uint8_t> result;
  result.push_back(frame.header);
  result.push_back(frame.control_code);
  result.push_back(frame.number);
  result.push_back(frame.frame_count);
  result.push_back(static_cast<uint8_t>(frame.length >> 8));    // 长度高字节
  result.push_back(static_cast<uint8_t>(frame.length & 0xFF));  // 长度低字节
  result.insert(result.end(), frame.data.begin(), frame.data.end());

  // 计算校验和（从控制码到数据域结束）
  uint8_t checksum = 0;
  for (size_t i = 1; i < result.size(); ++i) {  // 跳过帧头
    checksum += result[i];
  }
  frame.checksum = checksum;

  // 添加校验和和帧尾
  result.push_back(frame.checksum);
  result.push_back(frame.tail);

  LOG(INFO) << "编码帧: " << BytesToHexString(result);
  return result;
}

bool Protocol::Decode(const std::vector<uint8_t>& raw_data,
                      ProtocolFrame& frame) {
  // 最小帧长度：帧头(1) + 控制码(1) + 编号(1) + 帧计数(1) + 长度(2) + 校验(1) +
  // 帧尾(1) = 8
  if (raw_data.size() < 8) {
    LOG(ERROR) << "帧长度不足: " << raw_data.size();
    return false;
  }

  // 验证帧头
  if (raw_data[0] != FRAME_HEADER) {
    LOG(ERROR) << "帧头错误: 0x" << std::hex << static_cast<int>(raw_data[0]);
    return false;
  }

  // 验证帧尾
  if (raw_data[raw_data.size() - 1] != FRAME_TAIL) {
    LOG(ERROR) << "帧尾错误: 0x" << std::hex
               << static_cast<int>(raw_data[raw_data.size() - 1]);
    return false;
  }

  // 解析字段
  size_t index = 0;
  frame.header = raw_data[index++];
  frame.control_code = raw_data[index++];
  frame.number = raw_data[index++];
  frame.frame_count = raw_data[index++];
  frame.length = (static_cast<uint16_t>(raw_data[index]) << 8) |
                 static_cast<uint16_t>(raw_data[index + 1]);
  index += 2;

  // 验证数据长度
  if (raw_data.size() < index + frame.length + 2) {  // +2 for checksum and tail
    LOG(ERROR) << "数据长度不匹配. 期望: " << frame.length
               << ", 实际: " << (raw_data.size() - index - 2);
    return false;
  }

  // 提取数据域
  frame.data.clear();
  frame.data.insert(frame.data.end(), raw_data.begin() + index,
                    raw_data.begin() + index + frame.length);
  index += frame.length;

  // 提取校验和
  frame.checksum = raw_data[index++];
  frame.tail = raw_data[index];

  // 验证校验和（从控制码到数据域结束）
  uint8_t calculated_checksum = 0;
  for (size_t i = 1; i < raw_data.size() - 2; ++i) {  // 跳过帧头、校验和、帧尾
    calculated_checksum += raw_data[i];
  }

  if (frame.checksum != calculated_checksum) {
    LOG(ERROR) << "校验和错误. 期望: 0x" << std::hex
               << static_cast<int>(calculated_checksum) << ", 实际: 0x"
               << static_cast<int>(frame.checksum);
    return false;
  }

  LOG(INFO) << "解码成功 - 控制码: 0x" << std::hex
            << static_cast<int>(frame.control_code)
            << ", 数据长度: " << std::dec << frame.length;
  return true;
}

uint8_t Protocol::CalculateChecksum(const std::vector<uint8_t>& data) {
  uint8_t checksum = 0;
  for (uint8_t byte : data) {
    checksum += byte;
  }
  return checksum;
}

bool Protocol::VerifyChecksum(const std::vector<uint8_t>& raw_data) {
  if (raw_data.size() < 8) {
    return false;
  }

  // 计算校验和（从控制码到数据域结束）
  uint8_t calculated_checksum = 0;
  for (size_t i = 1; i < raw_data.size() - 2; ++i) {
    calculated_checksum += raw_data[i];
  }

  uint8_t received_checksum = raw_data[raw_data.size() - 2];
  return calculated_checksum == received_checksum;
}

std::string Protocol::BytesToHexString(const std::vector<uint8_t>& bytes) {
  std::ostringstream oss;
  oss << std::hex << std::uppercase << std::setfill('0');
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (i > 0) oss << " ";
    oss << std::setw(2) << static_cast<int>(bytes[i]);
  }
  return oss.str();
}

std::vector<uint8_t> Protocol::HexStringToBytes(const std::string& hex_str) {
  std::vector<uint8_t> bytes;
  std::istringstream iss(hex_str);
  std::string byte_str;

  while (iss >> byte_str) {
    if (byte_str.length() == 2) {
      uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
      bytes.push_back(byte);
    }
  }

  return bytes;
}

std::string Protocol::BytesToBase64(const std::vector<uint8_t>& bytes) {
  static const char base64_chars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string result;
  int i = 0;
  uint8_t char_array_3[3];
  uint8_t char_array_4[4];

  for (uint8_t byte : bytes) {
    char_array_3[i++] = byte;
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] =
          ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] =
          ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; i < 4; i++) {
        result += base64_chars[char_array_4[i]];
      }
      i = 0;
    }
  }

  if (i > 0) {
    for (int j = i; j < 3; j++) {
      char_array_3[j] = '\0';
    }

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] =
        ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] =
        ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

    for (int j = 0; j < i + 1; j++) {
      result += base64_chars[char_array_4[j]];
    }

    while (i++ < 3) {
      result += '=';
    }
  }

  return result;
}

std::vector<uint8_t> Protocol::Base64ToBytes(const std::string& base64_str) {
  static const std::string base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::vector<uint8_t> result;
  int i = 0;
  uint8_t char_array_4[4], char_array_3[3];

  for (char c : base64_str) {
    if (c == '=') break;
    if (!isalnum(c) && c != '+' && c != '/') continue;

    char_array_4[i++] = c;
    if (i == 4) {
      for (i = 0; i < 4; i++) {
        char_array_4[i] =
            static_cast<uint8_t>(base64_chars.find(char_array_4[i]));
      }

      char_array_3[0] =
          (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] =
          ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; i < 3; i++) {
        result.push_back(char_array_3[i]);
      }
      i = 0;
    }
  }

  if (i > 0) {
    for (int j = i; j < 4; j++) {
      char_array_4[j] = 0;
    }

    for (int j = 0; j < 4; j++) {
      char_array_4[j] =
          static_cast<uint8_t>(base64_chars.find(char_array_4[j]));
    }

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] =
        ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

    for (int j = 0; j < i - 1; j++) {
      result.push_back(char_array_3[j]);
    }
  }

  return result;
}

uint8_t Protocol::CalculateFrameChecksum(const ProtocolFrame& frame) {
  uint8_t checksum = 0;
  checksum += frame.control_code;
  checksum += frame.number;
  checksum += frame.frame_count;
  checksum += static_cast<uint8_t>(frame.length >> 8);
  checksum += static_cast<uint8_t>(frame.length & 0xFF);

  for (uint8_t byte : frame.data) {
    checksum += byte;
  }

  return checksum;
}
