#include "amqp_wrapper.h"

#include <stdio.h>
#include <windows.h>

// 1. 连接到 RabbitMQ
amqp_connection_state_t connect_rabbitmq_server(char *user_name, char *user_password)
{
	amqp_connection_state_t conn;
	conn = amqp_new_connection();

	amqp_socket_t *socket = amqp_tcp_socket_new(conn);
	if (!socket) {
		die("creating TCP socket");
	}
	int status = amqp_socket_open(socket, "localhost", 5672);
	if (status) {
		die("opening TCP socket");
	}

	die_on_amqp_error(amqp_login(conn
		, AMQP_DEFAULT_VHOST
		, AMQP_DEFAULT_MAX_CHANNELS
		, AMQP_DEFAULT_FRAME_SIZE
		, AMQP_DEFAULT_HEARTBEAT
		, AMQP_SASL_METHOD_PLAIN
		, user_name
		, user_password),
		"Logging in");

	return conn;
}

// 2. 获得信道
void get_channel(amqp_connection_state_t conn, int channel_id)
{
	amqp_channel_open(conn, channel_id);
	die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel");
}

// 3. 声明交换器
void declare_exchange(amqp_connection_state_t conn
	, int channel_id
	, const char *exchange_name
	, const char *exchange_type
	, int durable	// 是否持久化
)
{
	amqp_exchange_declare(conn, channel_id, amqp_cstring_bytes(exchange_name),
		amqp_cstring_bytes(exchange_type), 0, durable, 0, 0,
		amqp_empty_table);
	die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening exchange");
}

// 4. 声明队列
amqp_queue_declare_ok_t * declare_queue(amqp_connection_state_t conn
	, int channel_id
	, const char *queue_name
	, int durable	// 是否持久化
	, int exclusive	// 是否私有化
)
{
	amqp_queue_declare_ok_t *r = amqp_queue_declare(
		conn, channel_id, amqp_cstring_bytes(queue_name), 0, durable, exclusive, 0, amqp_empty_table);
	die_on_amqp_error(amqp_get_rpc_reply(conn), "Declaring queue");

	return r;
}

// 5. 把队列和交换器绑定起来
void bind_queue_exchange(amqp_connection_state_t conn
	, int channel_id
	, amqp_bytes_t queue_name
	, amqp_bytes_t exchange_name
	, amqp_bytes_t bind_key)
{
	amqp_queue_bind(conn, channel_id, queue_name, exchange_name,
		bind_key, amqp_empty_table);
	die_on_amqp_error(amqp_get_rpc_reply(conn), "Binding queue");
}

// 7. 关闭信道
void close_channel(amqp_connection_state_t conn, int channel_id)
{
	amqp_channel_close(conn, channel_id, AMQP_REPLY_SUCCESS);
}

// 8. 关闭连接
void disconnect_rabbitmq_server(amqp_connection_state_t conn)
{
	amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
	amqp_destroy_connection(conn);
}


// 6. 产生信息
void push_message(amqp_connection_state_t conn
	, int channel_id
	, amqp_bytes_t exchange_name
	, amqp_bytes_t bind_key
	, int mandatory
	, int immediate
	, amqp_bytes_t message)
{
	amqp_basic_properties_t props;
	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
	props.content_type = amqp_cstring_bytes("text/plain");
	props.delivery_mode = AMQP_DELIVERY_PERSISTENT;
	die_on_error(amqp_basic_publish(conn, channel_id, exchange_name,
		bind_key, mandatory, immediate,
		&props, message), "Publishing");
}

// 6. 消费信息
void set_consume(amqp_connection_state_t conn, int channel_id, amqp_bytes_t queue,
	amqp_bytes_t consumer_tag, amqp_boolean_t no_local, amqp_boolean_t no_ack)
{
	amqp_basic_consume(conn, channel_id, queue, consumer_tag
		, no_local		// no_local
		, no_ack		// no_ack
		, 0,			// exclusive
		amqp_empty_table);
	die_on_amqp_error(amqp_get_rpc_reply(conn), "Consuming");
}

void consume_message(amqp_connection_state_t conn)
{
	for (;;) {
		amqp_rpc_reply_t res;
		amqp_envelope_t envelope;

		amqp_maybe_release_buffers(conn);

		res = amqp_consume_message(conn, &envelope, NULL, 0);

		if (AMQP_RESPONSE_NORMAL != res.reply_type) {
			break;
		}

		printf("Delivery tag %u, exchange %.*s routingkey %.*s\n",
			(unsigned)envelope.delivery_tag, (int)envelope.exchange.len,
			(char *)envelope.exchange.bytes, (int)envelope.routing_key.len,
			(char *)envelope.routing_key.bytes);

		if (envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
			printf("Content-type: %.*s\n",
				(int)envelope.message.properties.content_type.len,
				(char *)envelope.message.properties.content_type.bytes);
		}
		printf("----\n");

		// 模拟用户处理消息的耗时
		printf("Message: %.*s\n",
			(int)envelope.message.body.len,
			(char *)envelope.message.body.bytes);
		for (size_t i = 0; i < envelope.message.body.len; ++i)
		{
			char *pp = (char *)envelope.message.body.bytes;
			if (pp[i] == '.')
			{
				Sleep(1000);
			}
		}

		amqp_destroy_envelope(&envelope);

		//amqp_basic_ack(conn, config->channel_id, envelope.delivery_tag, 0);
	}
}