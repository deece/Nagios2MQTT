#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <nebmodules.h>
#include <neberrors.h>
#include <nebstructs.h>
#include <broker.h>
#include <nagios.h>
#include <nebcallbacks.h>
#include <mosquitto.h>
#include <cjson/cJSON.h>

#define DEFAULT_MQTT_HOST "localhost"
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_TOPIC_PREFIX "nagios"

#define RETRY_COUNT 3
#define RETRY_DELAY 1 // in seconds

static struct mosquitto *mosq = NULL;
static char mqtt_host[256] = DEFAULT_MQTT_HOST;
static int mqtt_port = DEFAULT_MQTT_PORT;
static char mqtt_username[256] = "";
static char mqtt_password[256] = "";
static char topic_prefix[256] = DEFAULT_TOPIC_PREFIX;

void *mqtt_module_handle = NULL;

NEB_API_VERSION(CURRENT_NEB_API_VERSION);

int nebmodule_init(int flags, char *args, nebmodule *handle);
int nebmodule_deinit(int flags, int reason);
int event_handler(int callback_type, void *data);

static void _log(int level, const char *format, va_list args) {
    char log_msg[512];
    vsnprintf(log_msg, sizeof(log_msg), format, args);
    write_to_all_logs(log_msg, level);
}

static void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    _log(NSLOG_INFO_MESSAGE, format, args);
    va_end(args);
}

static void log_warning(const char *format, ...) {
    va_list args;
    va_start(args, format);
    _log(NSLOG_RUNTIME_WARNING, format, args);
    va_end(args);
}

static void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    _log(NSLOG_RUNTIME_ERROR, format, args);
    va_end(args);
}

static void parse_args(char *args) {
    char *token;
    char *delim = " ";

    token = strtok(args, delim);
    while (token != NULL) {
        if (strncmp(token, "host=", 5) == 0) {
            strncpy(mqtt_host, token + 5, sizeof(mqtt_host) - 1);
        } else if (strncmp(token, "port=", 5) == 0) {
            mqtt_port = atoi(token + 5);
        } else if (strncmp(token, "username=", 9) == 0) {
            strncpy(mqtt_username, token + 9, sizeof(mqtt_username) - 1);
        } else if (strncmp(token, "password=", 9) == 0) {
            strncpy(mqtt_password, token + 9, sizeof(mqtt_password) - 1);
        } else if (strncmp(token, "prefix=", 7) == 0) {
            strncpy(topic_prefix, token + 7, sizeof(topic_prefix) - 1);
        }
        token = strtok(NULL, delim);
    }
}

void on_connect(struct mosquitto *mosq, void *obj, int rc) {
    if (rc == 0) {
        log_info("Successfully connected to MQTT broker.");
    } else {
        log_error("Failed to connect to MQTT broker, return code %d", rc);
    }
}

void on_disconnect(struct mosquitto *mosq, void *obj, int rc) {
    if (rc == 0) {
        log_info("Clean disconnection from MQTT broker.");
    } else {
        log_warning("Unexpected disconnection from MQTT broker, return code %d", rc);
    }
}

int nebmodule_init(int flags, char *args, nebmodule *handle) {
    int result;

    mqtt_module_handle = handle;

    /* Set module info */
    neb_set_module_info(handle, NEBMODULE_MODINFO_TITLE, "Nagios to MQTT");
    neb_set_module_info(handle, NEBMODULE_MODINFO_AUTHOR, "Alastair D'Silva");
    neb_set_module_info(handle, NEBMODULE_MODINFO_TITLE, "Copyright (c) 2024 Alastair D'Silva");
    neb_set_module_info(handle, NEBMODULE_MODINFO_VERSION, "0.1.0");
    neb_set_module_info(handle, NEBMODULE_MODINFO_LICENSE, "GPLv2");
    neb_set_module_info(handle, NEBMODULE_MODINFO_DESC, "Exports Nagios events via MQTT");
    log_info("Nagios to MQTT: Module loaded");

    // Parse arguments
    if (args != NULL) {
        parse_args(args);
    }

    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, true, NULL);

    if (!mosq) {
        log_error("Failed to initialize MQTT client.");
        return 1;
    }

    // Set username and password if provided
    if (strlen(mqtt_username) > 0 && strlen(mqtt_password) > 0) {
        mosquitto_username_pw_set(mosq, mqtt_username, mqtt_password);
    }

    // Set automatic reconnect options
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);

    result = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);
    if (result != MOSQ_ERR_SUCCESS) {
        log_error("Failed to connect to MQTT broker at %s:%d.", mqtt_host, mqtt_port);
        return 2;
    }

    mosquitto_loop_start(mosq); // Start the loop to handle reconnects

    // Register callbacks for Nagios events
    neb_register_callback(NEBCALLBACK_SERVICE_CHECK_DATA, handle, 0, event_handler);
    neb_register_callback(NEBCALLBACK_HOST_CHECK_DATA, handle, 0, event_handler);
    neb_register_callback(NEBCALLBACK_NOTIFICATION_DATA, handle, 0, event_handler);

    log_info("Nagios to MQTT initialized with broker at %s:%d.", mqtt_host, mqtt_port);

    return 0;
}

int nebmodule_deinit(int flags, int reason) {
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    log_info("Nagios to MQTT deinitialized.");

    return 0;
}

int event_handler(int callback_type, void *data) {
    cJSON *json;
    char *json_string;
    char topic[512];
    int result;

    switch (callback_type) {
        case NEBCALLBACK_SERVICE_CHECK_DATA: {
            nebstruct_service_check_data *alert = (nebstruct_service_check_data *)data;

            json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "type", "SERVICE_CHECK");
            cJSON_AddStringToObject(json, "host_name", alert->host_name);
            cJSON_AddStringToObject(json, "service_description", alert->service_description);
            cJSON_AddNumberToObject(json, "check_type", alert->check_type);
            cJSON_AddNumberToObject(json, "current_attempt", alert->current_attempt);
            cJSON_AddNumberToObject(json, "max_attempts", alert->max_attempts);
            cJSON_AddNumberToObject(json, "state_type", alert->state_type);
            cJSON_AddNumberToObject(json, "state", alert->state);
            cJSON_AddNumberToObject(json, "timeout", alert->timeout);
            cJSON_AddStringToObject(json, "command_name", alert->command_name);
            cJSON_AddStringToObject(json, "command_args", alert->command_args);
            cJSON_AddStringToObject(json, "command_line", alert->command_line);
            cJSON_AddNumberToObject(json, "start_time", alert->start_time.tv_sec);
            cJSON_AddNumberToObject(json, "end_time", alert->end_time.tv_sec);
            cJSON_AddNumberToObject(json, "early_timeout", alert->early_timeout);
            cJSON_AddNumberToObject(json, "execution_time", alert->execution_time);
            cJSON_AddNumberToObject(json, "latency", alert->latency);
            cJSON_AddNumberToObject(json, "return_code", alert->return_code);
            cJSON_AddStringToObject(json, "output", alert->output);
            cJSON_AddStringToObject(json, "long_output", alert->long_output);
            cJSON_AddStringToObject(json, "perf_data", alert->perf_data);

            snprintf(topic, sizeof(topic), "%s/service_checks/%s/%s", topic_prefix, alert->host_name, alert->service_description);
            break;
        }

        case NEBCALLBACK_HOST_CHECK_DATA: {
            nebstruct_host_check_data *alert = (nebstruct_host_check_data *)data;

            json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "type", "HOST_CHECK");
            cJSON_AddStringToObject(json, "host_name", alert->host_name);
            cJSON_AddNumberToObject(json, "current_attempt", alert->current_attempt);
            cJSON_AddNumberToObject(json, "check_type", alert->check_type);
            cJSON_AddNumberToObject(json, "max_attempts", alert->max_attempts);
            cJSON_AddNumberToObject(json, "state_type", alert->state_type);
            cJSON_AddNumberToObject(json, "state", alert->state);
            cJSON_AddNumberToObject(json, "timeout", alert->timeout);
            cJSON_AddStringToObject(json, "command_name", alert->command_name);
            cJSON_AddStringToObject(json, "command_args", alert->command_args);
            cJSON_AddStringToObject(json, "command_line", alert->command_line);
            cJSON_AddNumberToObject(json, "start_time", alert->start_time.tv_sec);
            cJSON_AddNumberToObject(json, "end_time", alert->end_time.tv_sec);
            cJSON_AddNumberToObject(json, "early_timeout", alert->early_timeout);
            cJSON_AddNumberToObject(json, "execution_time", alert->execution_time);
            cJSON_AddNumberToObject(json, "latency", alert->latency);
            cJSON_AddNumberToObject(json, "return_code", alert->return_code);
            cJSON_AddStringToObject(json, "output", alert->output);
            cJSON_AddStringToObject(json, "long_output", alert->long_output);
            cJSON_AddStringToObject(json, "perf_data", alert->perf_data);

            snprintf(topic, sizeof(topic), "%s/host_checks/%s", topic_prefix, alert->host_name);
            break;
        }

        case NEBCALLBACK_NOTIFICATION_DATA: {
            nebstruct_notification_data *notification = (nebstruct_notification_data *)data;

            json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "type", "NOTIFICATION");
            cJSON_AddStringToObject(json, "host_name", notification->host_name);
            cJSON_AddStringToObject(json, "service_description", notification->service_description ? notification->service_description : "");
            cJSON_AddNumberToObject(json, "notification_type", notification->notification_type);
            cJSON_AddNumberToObject(json, "reason_type", notification->reason_type);
            cJSON_AddNumberToObject(json, "state", notification->state);
            cJSON_AddNumberToObject(json, "start_time", notification->start_time.tv_sec);
            cJSON_AddNumberToObject(json, "end_time", notification->end_time.tv_sec);
            cJSON_AddStringToObject(json, "output", notification->output);
            cJSON_AddStringToObject(json, "ack_author", notification->ack_author ? notification->ack_author : "");
            cJSON_AddStringToObject(json, "ack_data", notification->ack_data ? notification->ack_data : "");
            cJSON_AddNumberToObject(json, "escalated", notification->escalated);
            cJSON_AddNumberToObject(json, "contacts_notified", notification->contacts_notified);

            snprintf(topic, sizeof(topic), "%s/notifications/%s/%s", topic_prefix, notification->host_name, notification->service_description);
            break;
        }

        default:
            log_warning("Nagios to MQTT: Unknown callback type received.");
            return 1;
    }

    // Convert the JSON object to a string
    json_string = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    // Attempt to publish the JSON string to the appropriate MQTT topic
    int retries = RETRY_COUNT;
    do {
        result = mosquitto_publish(mosq, NULL, topic, strlen(json_string), json_string, 0, false);
        if (result != MOSQ_ERR_SUCCESS) {
            log_warning("Nagios to MQTT: Failed to publish MQTT message. Retrying...");
            sleep(RETRY_DELAY);
        }
    } while (result != MOSQ_ERR_SUCCESS && --retries > 0);

    if (result != MOSQ_ERR_SUCCESS) {
        log_error("Nagios to MQTT: Failed to publish MQTT message after retries. Error: %d", result);
    }

    free(json_string);

    return 0;
}
