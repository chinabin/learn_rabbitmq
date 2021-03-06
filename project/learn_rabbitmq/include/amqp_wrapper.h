#pragma once

#include "amqp.h"
#include "amqp_tcp_socket.h"
#include "utils.h"

// 1. 连接到 RabbitMQ
amqp_connection_state_t connect_rabbitmq_server(char *user_name, char *user_password);

// 2. 获得信道
void get_channel(amqp_connection_state_t conn, int channel_id);

// 3. 声明交换器
void declare_exchange(amqp_connection_state_t conn, int channel_id, const char *exchange_name, const char *exchange_type, int durable);

// 4. 声明队列
amqp_queue_declare_ok_t * declare_queue(amqp_connection_state_t conn, int channel_id, const char *queue_name, int durable, int exclusive);

// 5. 把队列和交换器绑定起来
void bind_queue_exchange(amqp_connection_state_t conn, int channel_id, amqp_bytes_t queue_name, amqp_bytes_t exchange_name, amqp_bytes_t bind_key);
	
// 7. 关闭信道
void close_channel(amqp_connection_state_t conn, int channel_id);

// 8. 关闭连接
void disconnect_rabbitmq_server(amqp_connection_state_t conn);



// ---------------- 生产者 ----------------

struct config_for_producer
{
	char *host;
	int port;

	char *user_name;
	char *user_password;

	int channel_id;

	char *exchange_name;
	char *exchange_type;
	int exchange_durable;

	char *queue_name;
	int queue_durable;
	int queue_exclusive;

	char *bind_key;
	int mandatory;
	int immediate;

	char *message;
};

// 6. 产生信息
void push_message(amqp_connection_state_t conn, int channel_id, amqp_bytes_t exchange_name, amqp_bytes_t bind_key, int mandatory, int immediate, amqp_bytes_t message);



// ---------------- 消费者 ----------------

struct config_for_consumer
{
	char *host;
	int port;

	char *user_name;
	char *user_password;

	int channel_id;

	char *exchange_name;

	char *queue_name;
	int queue_durable;
	int queue_exclusive;

	int no_local;
	int no_ack;

	char *bind_key;
	int mandatory;
	int immediate;
};

void set_consume(amqp_connection_state_t conn, int channel_id, amqp_bytes_t queue,
	amqp_bytes_t consumer_tag, amqp_boolean_t no_local, amqp_boolean_t no_ack);

void consume_message(amqp_connection_state_t conn);
int get_envelope(amqp_connection_state_t conn, amqp_envelope_t *enve);



// ---------------- 通用函数 ----------------
int message_result(amqp_connection_state_t conn);

void set_channel_confirm(amqp_connection_state_t conn, int channel_id);