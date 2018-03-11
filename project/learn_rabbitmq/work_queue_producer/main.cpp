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
		printf("%s������: %s\n", head, queue_name);
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

	char *message;
};

int producer(config_for_producer *config)
{
	if (pre_check("������", config->exchange_name, config->queue_name, config->bind_key))
	{
		return 1;
	}

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
		printf("�����߶�������: %.*s\n", (int)queue_name.len, (char *)queue_name.bytes);
	}

	//��
	{
		if (*config->exchange_name != NULL)
		{
			amqp_queue_bind(conn, channel_id, queue_name, amqp_cstring_bytes(config->exchange_name),
				amqp_cstring_bytes(config->bind_key), amqp_empty_table);
			die_on_amqp_error(amqp_get_rpc_reply(conn), "Binding queue");
		}
	}

	// ������Ϣ
	const char *exchange_name = config->exchange_name;
	const char *message = config->message;
	{
		amqp_basic_properties_t props;
		props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
		props.content_type = amqp_cstring_bytes("text/plain");
		props.delivery_mode = AMQP_DELIVERY_PERSISTENT; /* persistent delivery mode */
		die_on_error(amqp_basic_publish(conn, channel_id, amqp_cstring_bytes(exchange_name),
			amqp_cstring_bytes(config->bind_key)
			, 0			// mandatory ���޷��ҵ� queue �洢��Ϣ����ô broker ����� basic.return ����Ϣ������������
			, 0			// immediate �� exchange �ڽ���Ϣ route �� queue(s) ʱ���ֶ�Ӧ�� queue ��û�������ߣ���ô������Ϣ��ͨ��basic.return����������������
			, &props, amqp_cstring_bytes(message)),
			"Publishing");
	}
	// message_result(conn);

	// �ر�����
	{
		// �ر��ŵ�
		die_on_amqp_error(amqp_channel_close(conn, channel_id, AMQP_REPLY_SUCCESS),
			"Closing channel");
		// �ر�����
		die_on_amqp_error(amqp_connection_close(conn, AMQP_REPLY_SUCCESS),
			"Closing connection");
		// ������Դ
		die_on_error(amqp_destroy_connection(conn), "Ending connection");
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 5)
	{
		printf("work_queue_producer exchange_name queue_name bind_key message");
		return 1;
	}

	config_for_producer config;
	config.host = "localhost";
	config.port = 5672;
	config.user_name = "guest";
	config.user_password = "guest";
	config.channel_id = 1;

	config.exchange_name = argv[1];
	config.queue_name = argv[2];
	config.bind_key = argv[3];
	config.message = argv[4];

	producer(&config);

	return 0;
}