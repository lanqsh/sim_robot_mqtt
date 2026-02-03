#ifndef MQTT_CALLBACK_H_
#define MQTT_CALLBACK_H_

#include "mqtt/async_client.h"

class MqttCallback : public virtual mqtt::callback {
 public:
  void connection_lost(const std::string& cause) override;
  void message_arrived(mqtt::const_message_ptr msg) override;
  void delivery_complete(mqtt::delivery_token_ptr token) override;
};

#endif  // MQTT_CALLBACK_H_
