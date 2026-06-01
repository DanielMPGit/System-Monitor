#include <iostream>
#include <fstream>
#include <unordered_map>
#include "mqtt/async_client.h"

std::unordered_map<std::string, std::string> loadEnv(const std::string& path) {
    std::unordered_map<std::string, std::string> env;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        auto pos = line.find('=');
        if (pos != std::string::npos)
            env[line.substr(0, pos)] = line.substr(pos + 1);
    }
    return env;
}

const std::string SERVER_ADDRESS("tcp://localhost:1883");
const std::string CLIENT_ID("cliente_cpp");

int main() {
    auto env = loadEnv(".env");

    mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);
    mqtt::connect_options connOpts;
    connOpts.set_user_name(env["MQTT_NAME"]);
    connOpts.set_password(env["MQTT_PASS"]);

    try {
        client.connect(connOpts)->wait();
        std::string payload = "Hola con auth!";
        client.publish("test/topic", payload.c_str(), payload.size(), 1, false);
        client.disconnect()->wait();
    }
    catch (const mqtt::exception& exc) {
        std::cerr << "Error: " << exc.what() << std::endl;
    }
    return 0;
}