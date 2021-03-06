#include "amqp_wrapper.h"

#include <stdio.h>
#include <windows.h>

#pragma warning (disable:4996)

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

// 6. 消费信息
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
	}
}

int message_result(amqp_connection_state_t conn)
{
	amqp_frame_t frame;
	amqp_rpc_reply_t ret;

	if (AMQP_STATUS_OK != amqp_simple_wait_frame(conn, &frame)) {
		return 0;
	}

	if (AMQP_FRAME_METHOD == frame.frame_type) {
		amqp_method_t method = frame.payload.method;
		fprintf(stdout, "method.id=%08X,method.name=%s\n",
			method.id, amqp_method_name(method.id));
		switch (method.id) {
		case AMQP_BASIC_ACK_METHOD:
			/* if we've turned publisher confirms on, and we've published a message
			* here is a message being confirmed
			*/
		{
			amqp_basic_ack_t *s;
			s = (amqp_basic_ack_t *)method.decoded;
			fprintf(stdout, "Ack.delivery_tag=%d\n", (int)s->delivery_tag);
			fprintf(stdout, "Ack.multiple=%d\n", s->multiple);
		}

		break;

		case AMQP_BASIC_NACK_METHOD:
			/* if we've turned publisher confirms on, and we've published a message
			* here is a message not being confirmed
			*/
		{
			amqp_basic_nack_t *s;
			s = (amqp_basic_nack_t *)method.decoded;
			fprintf(stdout, "NAck.delivery_tag=%d\n", (int)s->delivery_tag);
			fprintf(stdout, "NAck.multiple=%d\n", s->multiple);
			fprintf(stdout, "NAck.requeue=%d\n", s->requeue);
		}

		break;

		case AMQP_BASIC_RETURN_METHOD:
			/* if a published message couldn't be routed and the mandatory flag was set
			* this is what would be returned. The message then needs to be read.
			*/
		{
			amqp_message_t message;
			amqp_basic_return_t *s;
			char str[1024];
			s = (amqp_basic_return_t *)method.decoded;
			fprintf(stdout, "Return.reply_code=%d\n", s->reply_code);
			strncpy(str, (const char *)s->reply_text.bytes, s->reply_text.len); str[s->reply_text.len] = 0;
			fprintf(stdout, "Return.reply_text=%s\n", str);

			ret = amqp_read_message(conn, frame.channel, &message, 0);
			if (AMQP_RESPONSE_NORMAL != ret.reply_type) {
				return 0;
			}
			strncpy(str, (const char *)message.body.bytes, message.body.len); str[message.body.len] = 0;
			fprintf(stdout, "Return.message=%s\n", str);

			amqp_destroy_message(&message);
		}

		break;

		case AMQP_CHANNEL_CLOSE_METHOD:
			/* a channel.close method happens when a channel exception occurs, this
			* can happen by publishing to an exchange that doesn't exist for example
			*
			* In this case you would need to open another channel redeclare any queues
			* that were declared auto-delete, and restart any consumers that were attached
			* to the previous channel
			*/
			return 0;

		case AMQP_CONNECTION_CLOSE_METHOD:
			/* a connection.close method happens when a connection exception occurs,
			* this can happen by trying to use a channel that isn't open for example.
			*
			* In this case the whole connection must be restarted.
			*/
			return 0;

		default:
			fprintf(stderr, "An unexpected method was received %d\n", frame.payload.method.id);
			return 1;
		}
	}

	return 0;
}

void set_channel_confirm(amqp_connection_state_t conn, int channel_id)
{
	amqp_confirm_select(conn, channel_id);
	die_on_amqp_error(amqp_get_rpc_reply(conn), "set channel confirm");
}

int get_envelope(amqp_connection_state_t conn, amqp_envelope_t *envelope)
{
	amqp_rpc_reply_t res;

	amqp_maybe_release_buffers(conn);

	res = amqp_consume_message(conn, envelope, NULL, 0);

	if (AMQP_RESPONSE_NORMAL != res.reply_type) {
		return 1;
	}
	return 0;
}