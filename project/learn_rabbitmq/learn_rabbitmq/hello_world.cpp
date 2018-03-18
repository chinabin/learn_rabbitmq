#include "amqp.h"
#include "amqp_tcp_socket.h"
#include "utils.h"

#include <stdio.h>
#include <windows.h>

#pragma warning (disable:4996)

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

struct config_for_producer
{
	char *host;
	int port;

	char *user_name;
	char *user_password;

	int channel_id;
	char *exchange_name;
	char *queue_name;
	char *bind_key;
};

int pre_check(char *head, char *exchange_name, char *queue_name, char *bind_key)
{
	bool has_exchange_name = false;
	if (exchange_name && *exchange_name != '\0')
	{
		has_exchange_name = true;
	}
	bool has_queue_name = false;
	if (queue_name && *queue_name != '\0')
	{
		has_queue_name = true;
	}
	bool has_bind_key = false;
	if (bind_key && *bind_key != '\0')
	{
		has_bind_key = true;
	}


	char pre_tip[300] = "";
	if (!has_exchange_name)
	{
		sprintf(pre_tip, "%s使用默认交换器，", head);
	}
	else
	{
		sprintf(pre_tip, "%s使用交换器%s，", head, exchange_name);
	}

	if (!has_queue_name && !has_bind_key)
	{
		printf("%s没有明确的绑定标识，使用随机的队列名作为绑定键怕你顶不住\n", pre_tip);
		return 1;
	}
	else if (has_bind_key)		// 判断必须位于 queue_name 的校验之前才准确
	{
		printf("%s绑定键明确设置: %s\n", pre_tip, bind_key);
	}
	else if (has_queue_name)
	{
		printf("%s使用队列名作为绑定键: %s\n", pre_tip, queue_name);
	}

	return 0;
}

/*
 生产者使用的是默认的交换器，需要设置正确的 binding_key 才能使得消息到达队列。
 因为消费者也是使用默认的交换器，并且消费者创建了队列，由于 AMQP 存在一
 个特点： AMQP 默认创建了一些交换器，其中一个是 direct 类型的交换器，名字
 是 amq.direct ，当创建队列的时候，会默认绑定到这个交换器，并且设置 route_key
 等于队列名。
 所以，生产者需要设置 bingding_key 为消费者创建的队列名，才能使得消息被正确
 路由到队列。
*/
DWORD WINAPI producer(LPVOID lpParam)
{
	config_for_producer *config = (config_for_producer *)lpParam;

	if (pre_check("生产者", config->exchange_name, config->queue_name, config->bind_key))
	{
		return 1;
	}

	amqp_connection_state_t conn;
	// 连接并登录到 RabbitMQ 服务器
	{
		conn = amqp_new_connection();

		amqp_socket_t *socket = amqp_tcp_socket_new(conn);
		if (!socket) {
			die("creating TCP socket");
		}
		int status = amqp_socket_open(socket, config->host, config->port);
		if (status) {
			die("opening TCP socket");
		}

		die_on_amqp_error(amqp_login(conn, "/", 0, AMQP_DEFAULT_FRAME_SIZE, 0, AMQP_SASL_METHOD_PLAIN,
			config->user_name, config->user_password),
			"Logging in");
	}

	// 获取信道
	int channel_id = config->channel_id;
	{
		amqp_channel_open(conn, channel_id);
		die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel");
	}

	// 确保有队列可以接收消息
	amqp_bytes_t queue_name;
	{
		amqp_bytes_t queue_info;
		if (*config->queue_name != NULL)
		{
			queue_info = amqp_cstring_bytes(config->queue_name);
		}
		else
		{
			queue_info = amqp_empty_bytes;
		}

		amqp_queue_declare_ok_t *r = amqp_queue_declare(
			conn, channel_id, queue_info
			, 0		// passive
			, 1		// durable
			, 0		// exclusive
			, 0		// auto_delete
			, amqp_empty_table);
		die_on_amqp_error(amqp_get_rpc_reply(conn), "Declaring queue");
		queue_name = amqp_bytes_malloc_dup(r->queue);
		//printf("生产者队列名称: %.*s\n", (int)queue_name.len, (char *)queue_name.bytes);
	}

	//绑定
	{
		// from https://www.rabbitmq.com/tutorials/amqp-concepts.html#exchange-default
		/*
		The default exchange is a direct exchange with no name (empty string) pre-declared by the broker.
		It has one special property that makes it very useful for simple applications: every queue that
		is created is automatically bound to it with a routing key which is the same as the queue name.
		*/
		if (*config->exchange_name != NULL)
		{
			amqp_queue_bind(conn, channel_id, queue_name, amqp_cstring_bytes(config->exchange_name),
				amqp_cstring_bytes(config->bind_key), amqp_empty_table);
			die_on_amqp_error(amqp_get_rpc_reply(conn), "Binding queue");
		}
	}

	// 推送消息
	const char *exchange_name = config->exchange_name;
	const char *message = "te谭斌st";
	{
		amqp_basic_properties_t props;
		props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
		props.content_type = amqp_cstring_bytes("text/plain");
		props.delivery_mode = AMQP_DELIVERY_PERSISTENT; /* persistent delivery mode */
		die_on_error(amqp_basic_publish(conn, channel_id, amqp_cstring_bytes(exchange_name),
			amqp_cstring_bytes(config->bind_key)
			, 1			// mandatory ，无法找到 queue 存储消息，那么 broker 会调用 basic.return 将消息返还给生产者
			, 0			// immediate ， exchange 在将消息 route 到 queue(s) 时发现对应的 queue 上没有消费者，那么这条消息会通过basic.return方法返还给生产者
			, &props, amqp_cstring_bytes(message)),
			"Publishing");
	}
	// message_result(conn);

	// 关闭清理
	{
		// 关闭信道
		die_on_amqp_error(amqp_channel_close(conn, channel_id, AMQP_REPLY_SUCCESS),
			"Closing channel");
		// 关闭连接
		die_on_amqp_error(amqp_connection_close(conn, AMQP_REPLY_SUCCESS),
			"Closing connection");
		// 清理资源
		die_on_error(amqp_destroy_connection(conn), "Ending connection");
	}

	return 0;
}

struct config_for_consumer
{
	char *host;
	int port;

	char *user_name;
	char *user_password;

	int channel_id;
	char *exchange_name;
	char *queue_name;
	char *bind_key;
};

DWORD WINAPI consumer(LPVOID lpParam)
{
	config_for_consumer *config = (config_for_consumer *)lpParam;
	amqp_connection_state_t conn;
	// 连接并登录到 RabbitMQ 服务器
	{
		conn = amqp_new_connection();

		amqp_socket_t *socket = amqp_tcp_socket_new(conn);
		if (!socket) {
			die("creating TCP socket");
		}
		int status = amqp_socket_open(socket, config->host, config->port);
		if (status) {
			die("opening TCP socket");
		}

		die_on_amqp_error(amqp_login(conn, "/", 0, AMQP_DEFAULT_FRAME_SIZE, 0, AMQP_SASL_METHOD_PLAIN,
			config->user_name, config->user_password),
			"Logging in");
	}

	// 获取信道
	int channel_id = config->channel_id;
	{
		amqp_channel_open(conn, channel_id);
		die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel");
	}

	// 确保有队列可以接收消息
	amqp_bytes_t queue_name;
	{
		amqp_queue_declare_ok_t *r = amqp_queue_declare(
			conn, channel_id, amqp_cstring_bytes(config->queue_name)
			, 0		// passive
			, 1		// durable
			, 0		// exclusive
			, 0		// auto_delete
			, amqp_empty_table);
		die_on_amqp_error(amqp_get_rpc_reply(conn), "Declaring queue");
		queue_name = amqp_bytes_malloc_dup(r->queue);
		//printf("消费者队列名称: %.*s\n", (int)queue_name.len, (char *)queue_name.bytes);
	}

	//绑定
	{
		// from https://www.rabbitmq.com/tutorials/amqp-concepts.html#exchange-default
		/*
		 The default exchange is a direct exchange with no name (empty string) pre-declared by the broker. 
		 It has one special property that makes it very useful for simple applications: every queue that 
		 is created is automatically bound to it with a routing key which is the same as the queue name.
		*/
		if (*config->exchange_name != NULL)
		{
			amqp_queue_bind(conn, channel_id, queue_name, amqp_cstring_bytes(config->exchange_name),
				amqp_cstring_bytes(config->bind_key), amqp_empty_table);
			die_on_amqp_error(amqp_get_rpc_reply(conn), "Binding queue");
		}
	}

	{
		// amqp_basic_consume 属于被动接收消息，由服务端主动推送
		// 消息只是到达了本地，还需要调用 amqp_consume_message 从本地获取到消息
		amqp_basic_consume(conn, channel_id, queue_name, amqp_cstring_bytes(config->exchange_name)
			, 0		// no_local
			, 1		// no_ack
			, 0,	// exclusive
			amqp_empty_table);
		die_on_amqp_error(amqp_get_rpc_reply(conn), "Consuming");
		
		for (;;) {
			amqp_rpc_reply_t res;
			amqp_envelope_t envelope;	// 定义信封载体
		
			amqp_maybe_release_buffers(conn);	// https://github.com/alanxz/rabbitmq-c/issues/211
		
			// 把收到的相关信息放入信封中
			res = amqp_consume_message(conn, &envelope, NULL, 0);
		
			if (AMQP_RESPONSE_NORMAL != res.reply_type) {
				break;
			}
		
			printf("Delivery %u, exchange %.*s routingkey %.*s\n",
				(unsigned)envelope.delivery_tag, (int)envelope.exchange.len,
				(char *)envelope.exchange.bytes, (int)envelope.routing_key.len,
				(char *)envelope.routing_key.bytes);
		
			if (envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
				printf("Content-type: %.*s\n",
					(int)envelope.message.properties.content_type.len,
					(char *)envelope.message.properties.content_type.bytes);
			}
			printf("----\n");
		
			printf("Message: %.*s\n",
				(int)envelope.message.body.len,
				(char *)envelope.message.body.bytes);
		
			amqp_destroy_envelope(&envelope);
		}
	}

	return 0;
}

int main()
{
	{
		config_for_producer p_config;
		p_config.host = "localhost";
		p_config.port = 5672;
		p_config.user_name = "guest";
		p_config.user_password = "guest";
		p_config.channel_id = 11;
		p_config.exchange_name = "";
		p_config.queue_name = "queue_test";
		p_config.bind_key = "queue_test";
		CreateThread(NULL, 0, producer, &p_config, 0, NULL);
	}

	{
		config_for_consumer c_config;
		c_config.host = "localhost";
		c_config.port = 5672;
		c_config.user_name = "guest";
		c_config.user_password = "guest";
		c_config.channel_id = 10;
		c_config.exchange_name = "";
		c_config.queue_name = "queue_test";
		c_config.bind_key = "";
		CreateThread(NULL, 0, consumer, &c_config, 0, NULL);
	}

	while (1)
	{
		Sleep(50);
	}
	return 0;
}