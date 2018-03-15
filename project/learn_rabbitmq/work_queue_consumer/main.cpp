/* 消费者代码 */
#include "amqp_wrapper.h"

#include <stdio.h>
#include <windows.h>

struct config_for_consumer
{
	char *host;
	int port;

	char *user_name;
	char *user_password;

	int channel_id;

	char *queue_name;
	int queue_durable;
	int queue_exclusive;

	int no_local;
	int no_ack;

	char *bind_key;
	int mandatory;
	int immediate;
};

int consumer(config_for_consumer *config)
{
	// 连接并登录到 RabbitMQ 服务器
	amqp_connection_state_t conn = connect_rabbitmq_server(config->user_name, config->user_password);

	// 获取信道
	get_channel(conn, config->channel_id);

	// 确保有队列可以接收消息
	amqp_queue_declare_ok_t *r = declare_queue(conn, config->channel_id, config->queue_name, config->queue_durable, config->queue_exclusive);
	amqp_bytes_t queue_name = amqp_bytes_malloc_dup(r->queue);
	printf("消费者 队列名称: %.*s\n", (int)queue_name.len, (char *)queue_name.bytes);

	// amqp_basic_qos(conn, config->channel_id, 0, 1, 0);

	{
		set_consume(conn, config->channel_id, queue_name, amqp_empty_bytes, config->no_local, config->no_ack);
		consume_message(conn);
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("work_queue_consumer queue_name bind_key");
		return 1;
	}

	config_for_consumer config;
	config.host = "localhost";
	config.port = 5672;
	config.user_name = "guest";
	config.user_password = "guest";
	config.channel_id = 1;

	config.queue_name = argv[1];
	config.bind_key = argv[2];

	consumer(&config);

	return 0;
}