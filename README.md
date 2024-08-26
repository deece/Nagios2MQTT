
# Nagios MQTT NEB Module

This Nagios Event Broker (NEB) module captures Nagios events such as service alerts, host alerts, and notifications, and publishes them as JSON-formatted messages to a specified MQTT broker.

## Features

- Publishes Nagios events to different MQTT topics based on event type:
  - Service Alerts: `nagios/service_alerts`
  - Host Alerts: `nagios/host_alerts`
  - Notifications: `nagios/notifications`
- Outputs events in JSON format, including all relevant alert details.
- Configurable MQTT broker connection parameters (host, port, username, password) at runtime via the Nagios configuration file.

## Requirements

- Nagios Core
- Mosquitto MQTT broker or any other compatible MQTT broker
- The following libraries:
  - `libmosquitto-dev`
  - `libcjson-dev`

## Installation

### Step 1: Clone the Repository

Clone this repository or copy the source code files to your server.

### Step 2: Install Dependencies

Ensure all required dependencies are installed. You can install them using the following command:

\`\`\`bash
make deps
\`\`\`

### Step 3: Build the NEB Module

Compile the NEB module by running:

\`\`\`bash
make
\`\`\`

This will produce a shared object file named `nagios2mqtt.so`.

### Step 4: Install the NEB Module

Install the module to the Nagios plugins directory:

\`\`\`bash
sudo make install
\`\`\`

### Step 5: Configure Nagios

Edit the Nagios configuration file (`nagios.cfg`) to load the NEB module and configure the MQTT connection parameters:

\`\`\`ini
broker_module=/usr/lib/nagios/plugins/nagios2mqtt.so host=mqtt.example.com port=1883 username=myuser password=mypassword prefix=topic_prefix
\`\`\`

Replace `mqtt.example.com`, `1883`, `myuser`, and `mypassword` with your actual MQTT broker's detail. Information will be posted under the specified topic prefix, which defaults to 'nagios'.

### Step 6: Restart Nagios

Restart the Nagios service to apply the changes:

\`\`\`bash
sudo systemctl restart nagios
\`\`\`

## Configuration

### Command-Line Arguments

You can configure the following MQTT connection parameters via command-line arguments in the `nagios.cfg` file:

- `host`: The MQTT broker's hostname or IP address.
- `port`: The MQTT broker's port (default is `1883`).
- `username`: The username for MQTT authentication (optional).
- `password`: The password for MQTT authentication (optional).

Example configuration in `nagios.cfg`:

\`\`\`ini
broker_module=/usr/lib/nagios/plugins/nagios2mqtt.so host=mqtt.example.com port=1883 username=myuser password=mypassword
\`\`\`

### Default Values

If no arguments are provided, the module defaults to:

- `host`: `localhost`
- `port`: `1883`
- `username`: (empty)
- `password`: (empty)

## Uninstallation

To uninstall the NEB module, run:

\`\`\`bash
sudo make uninstall
\`\`\`

Remove the module's configuration from the `nagios.cfg` file and restart Nagios.

## License

This project is licensed under the LGPLv3 License. See the `LICENSE` file for more details.

## Contributing

Contributions are welcome! Feel free to submit a pull request or open an issue for any bugs or feature requests.
