///* �����ߴ��� */
//
//#include "amqp_wrapper.h"
//
//#include <stdio.h>
//
//
//int producer(config_for_producer *config)
//{
//	char *exchange_name = config->exchange_name;
//	// ���Ӳ���¼�� RabbitMQ ������
//	amqp_connection_state_t conn = connect_rabbitmq_server(config->user_name, config->user_password);
//
//	// ��ȡ�ŵ�
//	get_channel(conn, config->channel_id);
//
//	// ����������
//	declare_exchange(conn, config->channel_id, config->exchange_name
//		, config->exchange_type, config->exchange_durable);
//
//	// ��������
//	amqp_bytes_t queue_name;
//	amqp_queue_declare_ok_t *r = declare_queue(conn, config->channel_id, config->queue_name, config->queue_durable, config->queue_exclusive);
//	queue_name = amqp_bytes_malloc_dup(r->queue);
//	printf("������ ��������: %.*s\n", (int)queue_name.len, (char *)queue_name.bytes);
//	
//	// ��
//	bind_queue_exchange(conn, config->channel_id, queue_name, amqp_cstring_bytes(config->exchange_name), amqp_cstring_bytes(config->bind_key));
//
//	// ������Ϣ
//	push_message(conn, config->channel_id
//		, amqp_cstring_bytes(config->exchange_name)
//		, amqp_cstring_bytes(config->bind_key)
//		, config->mandatory
//		, config->immediate, amqp_cstring_bytes(config->message));
//	// message_result(conn);
//
//	// �ر�����
//	{
//		// �ر��ŵ�
//		close_channel(conn, config->channel_id);
//		// �ر�����
//		disconnect_rabbitmq_server(conn);
//	}
//
//	return 0;
//}
//
//int main(int argc, char **argv)
//{
//	if (argc != 5)
//	{
//		printf("work_queue_producer exchange_name queue_name bind_key message");
//		return 1;
//	}
//
//	config_for_producer config;
//	config.host = "localhost";
//	config.port = 5672;
//	config.user_name = "guest";
//	config.user_password = "guest";
//	config.channel_id = 1;
//
//	config.exchange_name = argv[1];
//	config.exchange_type = "direct";
//	config.exchange_durable = 1;
//
//	config.queue_name = argv[2];
//	config.queue_durable = 1;
//	config.queue_exclusive = 0;
//
//	config.bind_key = argv[3];
//	config.mandatory = 0;
//	config.immediate = 0;
//
//	config.message = argv[4];
//
//	producer(&config);
//
//	return 0;
//}