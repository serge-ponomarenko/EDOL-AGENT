#pragma once

class EdolMqttClient;

class SetupPortal
{
public:
    void begin(EdolMqttClient *mqtt = nullptr);
    void loop();

private:
    void registerRoutes();
};
