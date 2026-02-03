#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

#include "mqtt/async_client.h"

using namespace std::chrono_literals;

const std::string DEFAULT_BROKER("tcp://test.mosquitto.org:1883");
const std::string TELEMETRY_TOPIC("sim/robot/telemetry");
const std::string CONTROL_TOPIC("sim/robot/control");

class simple_callback : public virtual mqtt::callback
{
public:
    void connection_lost(const std::string &cause) override {
        std::cout << "Connection lost: " << cause << std::endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        std::cout << "[CONTROL] Topic: " << msg->get_topic() << " Payload: " << msg->to_string() << std::endl;
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}
};

int main(int argc, char* argv[])
{
    std::string broker = DEFAULT_BROKER;
    int duration = 30; // seconds

    if (argc >= 2) broker = argv[1];
    if (argc >= 3) duration = std::stoi(argv[2]);

    const std::string client_id = "sim_robot_cpp_" + std::to_string(std::hash<std::string>{}(broker));

    mqtt::async_client client(broker, client_id);
    simple_callback cb;
    client.set_callback(cb);

    mqtt::connect_options connOpts;

    try {
        std::cout << "Connecting to broker: " << broker << std::endl;
        client.connect(connOpts)->wait();
        std::cout << "Connected. Subscribing to control topic." << std::endl;
        client.subscribe(CONTROL_TOPIC, 1)->wait();

        std::cout << "Publishing telemetry for " << duration << " seconds." << std::endl;
        for (int i = 0; i < duration; ++i) {
            std::string payload = "{\"seq\": " + std::to_string(i) + ", \"battery\": " + std::to_string(100 - i % 100) + "}";
            auto msg = mqtt::make_message(TELEMETRY_TOPIC, payload);
            msg->set_qos(1);
            client.publish(msg);
            std::this_thread::sleep_for(1s);
        }

        std::cout << "Done publishing, disconnecting." << std::endl;
        client.disconnect()->wait();
    }
    catch (const mqtt::exception &exc) {
        std::cerr << "Error: " << exc.what() << std::endl;
        return 1;
    }

    return 0;
}
