///* �����ߴ��� */
//#include "amqp_wrapper.h"
//
//#include <stdio.h>
//#include <windows.h>
//
//int consumer(config_for_consumer *config)
//{
//	// ���Ӳ���¼�� RabbitMQ ������
//	amqp_connection_state_t conn = connect_rabbitmq_server(config->user_name, config->user_password);
//
//	// ��ȡ�ŵ�
//	get_channel(conn, config->channel_id);
//
//	// ȷ���ж��п��Խ�����Ϣ
//	amqp_queue_declare_ok_t *r = declare_queue(conn, config->channel_id, config->queue_name, config->queue_durable, config->queue_exclusive);
//	amqp_bytes_t queue_name = amqp_bytes_malloc_dup(r->queue);
//	printf("������ ��������: %.*s\n", (int)queue_name.len, (char *)queue_name.bytes);
//
//	// ��
//	bind_queue_exchange(conn, config->channel_id, queue_name, amqp_cstring_bytes(config->exchange_name), amqp_cstring_bytes(config->bind_key));
//
//	// amqp_basic_qos(conn, config->channel_id, 0, 1, 0);
//
//	{
//		set_consume(conn, config->channel_id, queue_name, amqp_empty_bytes, config->no_local, config->no_ack);
//		consume_message(conn);
//	}
//
//	return 0;
//}
//
//int main(int argc, char **argv)
//{
//	if (argc != 4)
//	{
//		printf("work_queue_consumer exchange_name queue_name bind_key");
//		return 1;
//	}
//
//	config_for_consumer config;
//	config.host = "localhost";
//	config.port = 5672;
//	config.user_name = "guest";
//	config.user_password = "guest";
//	config.channel_id = 1;
//
//	config.exchange_name = argv[1];
//	config.queue_name = argv[2];
//	config.queue_durable = 1;
//	config.queue_exclusive = 0;
//
//	config.no_local = 0;
//	config.no_ack = 1;
//
//	config.mandatory = 0;
//	config.immediate = 0;
//
//	config.bind_key = argv[3];
//
//	consumer(&config);
//
//	return 0;
//}