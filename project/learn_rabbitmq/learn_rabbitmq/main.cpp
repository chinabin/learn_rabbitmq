#include "amqp.h"
#include "amqp_tcp_socket.h"

// 1. 连接到 RabbitMQ
amqp_connection_state_t connect_rabbitmq_server()
{
	amqp_connection_state_t conn;
	conn = amqp_new_connection();

	amqp_socket_t *socket = amqp_tcp_socket_new(conn);
	int status = amqp_socket_open(socket, "localhost", 5672);

	amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
		"guest", "guest");

	return conn;
}

// 2. 获得信道

void get_channel(amqp_connection_state_t conn, int channel_id)
{
	amqp_channel_open(conn, channel_id);
	amqp_get_rpc_reply(conn);
}

// 3. 声明交换器
void declare_exchange(amqp_connection_state_t conn
	, int channel_id
	, const char *exchange_name
	, const char *exchange_type
	, int passive	// 检测是否存在
	, int durable	// 是否持久化
	, int auto_delete	// 是否自动删除
)
{
	amqp_exchange_declare(conn, channel_id, amqp_cstring_bytes(exchange_name),
		amqp_cstring_bytes(exchange_type), passive, durable, auto_delete, 0,
		amqp_empty_table);
	amqp_get_rpc_reply(conn);
}

// 4. 声明队列
amqp_queue_declare_ok_t * declare_queue(amqp_connection_state_t conn
	, int channel_id
	, int passive	// 检测是否存在
	, int durable	// 是否持久化
	, int exclusive	// 是否私有化
	, int auto_delete	// 是否自动删除
)
{
	amqp_queue_declare_ok_t *r = amqp_queue_declare(
		conn, channel_id, amqp_empty_bytes, passive, durable, exclusive, auto_delete, amqp_empty_table);
	amqp_get_rpc_reply(conn);

	return r;
}

// 5. 把队列和交换器绑定起来
void bind_queue_exchange(amqp_connection_state_t conn
	, int channel_id
	, amqp_bytes_t queue_name
	, const char *exchange_name
	, const char *bind_key)
{
	amqp_queue_bind(conn, 1, queue_name, amqp_cstring_bytes(exchange_name),
		amqp_cstring_bytes(bind_key), amqp_empty_table);
	amqp_get_rpc_reply(conn);
}

// 6. 产生信息
int push_message(amqp_connection_state_t conn
	, int channel_id
	, const char *exchange_name
	, const char *bind_key
	, int mandatory		// 
	, int immediate		// 
	, const char *message)
{
	amqp_basic_properties_t props;
	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
	props.content_type = amqp_cstring_bytes("text/plain");
	props.delivery_mode = AMQP_DELIVERY_PERSISTENT; /* persistent delivery mode */
	return amqp_basic_publish(conn, channel_id, amqp_cstring_bytes(exchange_name),
		amqp_cstring_bytes(bind_key), mandatory, immediate,
		&props, amqp_cstring_bytes(message));
}

// 7. 关闭信道
void close_channel(amqp_connection_state_t conn, int channel_id)
{
	amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
}

// 8. 关闭连接

void disconnect_rabbitmq_server(amqp_connection_state_t conn)
{
	amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
	amqp_destroy_connection(conn);
}

int main()
{
	amqp_connection_state_t conn = connect_rabbitmq_server();

	int channel_id = 1;
	get_channel(conn, channel_id);

	const char *exchange_name = "tanbin_test";
	declare_exchange(conn, channel_id, exchange_name, "direct", 0, 0, 0);

	amqp_queue_declare_ok_t *responce = declare_queue(conn, channel_id, 0, 0, 0, 0);
	amqp_bytes_t queue_name;
	queue_name = amqp_bytes_malloc_dup(responce->queue);

	const char *bind_key = "test";
	bind_queue_exchange(conn, channel_id, queue_name, exchange_name, bind_key);

	const char *message = "this is a test";
	int res = push_message(conn, channel_id, exchange_name, bind_key, 0, 0, message);

	close_channel(conn, channel_id);
	disconnect_rabbitmq_server(conn);

	return 0;
}