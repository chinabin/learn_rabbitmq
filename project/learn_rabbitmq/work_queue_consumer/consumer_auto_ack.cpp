/* �����ߴ��� */
#include "amqp_wrapper.h"

#include <stdio.h>
#include <windows.h>

int consumer(config_for_consumer *config)
{
	// ���Ӳ���¼�� RabbitMQ ������
	amqp_connection_state_t conn = connect_rabbitmq_server(config->user_name, config->user_password);

	// ��ȡ�ŵ�
	get_channel(conn, config->channel_id);

	// ȷ���ж��п��Խ�����Ϣ
	amqp_queue_declare_ok_t *r = declare_queue(conn, config->channel_id, config->queue_name, config->queue_durable, config->queue_exclusive);
	amqp_bytes_t queue_name = amqp_bytes_malloc_dup(r->queue);
	printf("������ ��������: %.*s\n", (int)queue_name.len, (char *)queue_name.bytes);

	// ��
	bind_queue_exchange(conn, config->channel_id, queue_name, amqp_cstring_bytes(config->exchange_name), amqp_cstring_bytes(config->bind_key));

	// ������Ϣ
	{
		set_consume(conn, config->channel_id, queue_name, amqp_empty_bytes, config->no_local, config->no_ack);

		printf("��ȷ��ȷ��ģʽ\n");
		return 1;
		// �Զ�ȷ��
		{
			consume_message(conn);
		}
		
		// �ֶ�ȷ��
		{
			for (;;)
			{
				amqp_envelope_t envelope;
				{
					if (get_envelope(conn, &envelope))
					{
						printf("get envelope failed\n");
						return 1;
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

					amqp_basic_ack(conn, config->channel_id, envelope.delivery_tag, 0);
					amqp_destroy_envelope(&envelope);
				}
			}
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		printf("work_queue_consumer exchange_name queue_name bind_key");
		return 1;
	}

	config_for_consumer config;
	config.host = "localhost";
	config.port = 5672;
	config.user_name = "guest";
	config.user_password = "guest";
	config.channel_id = 1;

	config.exchange_name = argv[1];
	config.queue_name = argv[2];
	config.queue_durable = 1;
	config.queue_exclusive = 0;

	config.no_local = 0;
	config.no_ack = 0;

	config.mandatory = 0;
	config.immediate = 0;

	config.bind_key = argv[3];

	consumer(&config);

	return 0;
}