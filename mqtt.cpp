#include <mqtt/async_client.h>
#include <iostream>

int main() {
    mqtt::create_options options;

    mqtt::async_client client(
        "tcp://test",
        "test_client",
        options
    );

    std::cout << "Paho MQTT funciona correctamente\n";

    return 0;
}
