#include "amqp.h"
#include "amqp_tcp_socket.h"
#include "utils.h"

#include <stdio.h>
#include <windows.h>


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

	if (!has_exchange_name)
	{
		printf("%s������: Ĭ��\n", head);
	}
	else
	{
		printf("%s������: %s\n", head, exchange_name);
	}

	if (!has_queue_name)
	{
		printf("%s������: ���\n", head);
	}
	else
	{
		printf("%s������: %s\n", head);
	}

	if (!has_queue_name && !has_bind_key)
	{
		printf("%s�󶨼�: ���\n", head);
	}
	else if (has_bind_key)		// �жϱ���λ�� queue_name ��У��֮ǰ��׼ȷ
	{
		printf("%s�󶨼�: %s\n", head, bind_key);
	}
	else if (has_queue_name)
	{
		printf("%s�󶨼�: %s(������)\n", head, queue_name);
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
	char *queue_name;
	char *bind_key;
};

int consumer(config_for_consumer *config)
{
	amqp_connection_state_t conn;
	// ���Ӳ���¼�� RabbitMQ ������
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

	// ��ȡ�ŵ�
	int channel_id = config->channel_id;
	{
		amqp_channel_open(conn, channel_id);
		die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel");
	}

	// ȷ���ж��п��Խ�����Ϣ
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
		//printf("�����߶�������: %.*s\n", (int)queue_name.len, (char *)queue_name.bytes);
	}

	{
		// amqp_basic_consume ���ڱ���������Ϣ���ɷ������������
		// ��Ϣֻ�ǵ����˱��أ�����Ҫ���� amqp_consume_message �ӱ��ػ�ȡ����Ϣ
		amqp_basic_consume(conn, channel_id, queue_name, amqp_empty_bytes
			, 0		// no_local
			, 0		// no_ack
			, 0,	// exclusive
			amqp_empty_table);
		die_on_amqp_error(amqp_get_rpc_reply(conn), "Consuming");

		for (;;) {
			amqp_rpc_reply_t res;
			amqp_envelope_t envelope;	// �����ŷ�����

			amqp_maybe_release_buffers(conn);	// https://github.com/alanxz/rabbitmq-c/issues/211

												// ���յ��������Ϣ�����ŷ���
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
			for (int i = 0; i < envelope.message.body.len; ++i)
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