

#ifndef MQTT_AGENT_MAX_OUTSTANDING_ACKS
    #define MQTT_AGENT_MAX_OUTSTANDING_ACKS    		(CONFIG_MQTT_AGENT_MAX_OUTSTANDING_ACKS)
#endif

/**
 * @brief Time in milliseconds that the MQTT agent task will wait in the Blocked state (so
 * not using any CPU time) for a command to arrive in its command queue before
 * exiting the blocked state so it can call MQTT_ProcessLoop().
 *
 * @note It is important MQTT_ProcessLoop() is called often if there is known
 * MQTT traffic, but calling it too often can take processing time away from
 * lower priority tasks and waste CPU time and power.
 *
 * <b>Possible values:</b> Any positive 32 bit integer. <br>
 * <b>Default value:</b> `1000`
 */
#ifndef MQTT_AGENT_MAX_EVENT_QUEUE_WAIT_TIME
    #define MQTT_AGENT_MAX_EVENT_QUEUE_WAIT_TIME	(CONFIG_MQTT_AGENT_MAX_EVENT_QUEUE_WAIT_TIME)
#endif

/**
 * @brief Whether the agent should configure the coreMQTT library to be used with publishes
 * greater than QoS0. Setting this to 0 will disallow the coreMQTT library to send publishes
 * with QoS > 0.
 *
 * <b>Possible values:</b> 0 or 1 <br>
 * <b>Default value:</b> `1`
 */
#ifndef MQTT_AGENT_USE_QOS_1_2_PUBLISH
#ifdef CONFIG_MQTT_AGENT_USE_QOS_1_2_PUBLISH
    #define MQTT_AGENT_USE_QOS_1_2_PUBLISH    (1)
#else
    #define MQTT_AGENT_USE_QOS_1_2_PUBLISH    (0)
#endif
#endif
