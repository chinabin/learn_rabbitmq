# RabbitMQ官方例子之HelloWorld例子笔记

标签（空格分隔）： RabbitMQ

---

###1. 生产者流程
生产者创建消息并推送的流程如下所示:

 - **连接 RabbitMQ 服务器**

        Q: 同一个用户可以多次登录吗？
        A: 没有找到明确的资料。个人想法是可以，原因如下： RabbitMQ 的账号和密码和我们平时理解的应该是不一样，它代表的更像是一种权限控制，例如账号A只能读，那么只要用账号A连接都只能读。换个角度，例如移动端使用 RabbitMQ ，那么就得创建成千上万个账号，这明显是不合理的。
 - **获取信道**
 
        Q: Channel 是否有数量限制？ 
        A: 肯定存在数量限制，我们的程序不会有那么大的消耗。
            如果想大量使用 Channel ，最好是控制好消息推送队列的粒度，然后设置不同的消费者去接收，并且每个消费者单独一个 Channel 。见图1.
        Q: Channel 一样会有什么影响吗？
        A: Channel 是和 connection 相关的，同一个连接中 Channel id不能一样，但是不同连接则可以一样。见下图2.
![snipaste20180307_233930.png](https://i.loli.net/2018/03/07/5aa007c25046b.png)
[图1出处][2]
![snipaste20180307_232738.png](https://i.loli.net/2018/03/07/5aa00521ac77f.png)
[图2出处][3]
 - **创建交换器**（首先查询所要创建的交换器是否已经存在）

		创建函数：
		AMQP_PUBLIC_FUNCTION
		amqp_exchange_declare_ok_t *AMQP_CALL amqp_exchange_declare(
		    amqp_connection_state_t state, amqp_channel_t channel,
		    amqp_bytes_t exchange, amqp_bytes_t type, amqp_boolean_t passive,
		    amqp_boolean_t durable, amqp_boolean_t auto_delete, amqp_boolean_t internal,
		    amqp_table_t arguments);
		结果确认：需要调用 amqp_get_rpc_reply

        名字：自己定义
        类型：direct、topic、Fanout
        passive：为 true 表示检测交换器是否存在，存在则返回成功，不存在则返回错误
        durable：是否持久化
        auto-delete：是否自动删除
        internal：
        arguments：未知用途
    
 - **创建队列**（首先查询所要创建的队列是否已经存在）

		创建函数：
		AMQP_PUBLIC_FUNCTION
		amqp_queue_declare_ok_t *AMQP_CALL amqp_queue_declare(
		    amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue,
		    amqp_boolean_t passive, amqp_boolean_t durable, amqp_boolean_t exclusive,
		    amqp_boolean_t auto_delete, amqp_table_t arguments);
		返回结果：需要调用 amqp_get_rpc_reply
		注意：可以将 passive 设置为 true 来明确检测将要创建的队列是否存在。但是队列的创建还有一个特性，就是说如果传入创建队列的参数一致，那么创建函数其实是什么也不会做，就好像当作创建成功一样。

		名字：自己定义，如果不指定的话 RabbitMQ 服务器默认会分配一个，并在返回结果中告诉。这对于创建私有化的队列很有用。
		passive：为 true 表示检测交换器是否存在，存在则返回成功，不存在则返回错误
		durable：是否持久化
		exclusive：是否私有化
		auto_delete：是否自动删除
		arguments：未知用途

 - **将交换器与队列绑定**

		创建函数：
		AMQP_PUBLIC_FUNCTION
		amqp_queue_bind_ok_t *AMQP_CALL amqp_queue_bind(
		    amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue,
		    amqp_bytes_t exchange, amqp_bytes_t routing_key, amqp_table_t arguments)
		队列名
		交换器名
		路由键

 - **推送消息**

		函数：
		AMQP_PUBLIC_FUNCTION
		int AMQP_CALL amqp_basic_publish(
		    amqp_connection_state_t state, amqp_channel_t channel,
		    amqp_bytes_t exchange, amqp_bytes_t routing_key, amqp_boolean_t mandatory,
		    amqp_boolean_t immediate, struct amqp_basic_properties_t_ const *properties,
		    amqp_bytes_t body);
		队列名
		路由键
		mandatory：置为 true 之后，如果消息没有找到可路由的队列，则会返回给客户端
		immediate：置为 true 之后，如果 exchange 在将消息 route 到 queue(s) 时发现对应的 queue 上没有消费者，那么这条消息不会放入队列中，该消息会通过basic.return方法返还给生产者
		properties：设置消息属性
		body：消息内容
 - **关闭连接**

		关闭通道：
			AMQP_PUBLIC_FUNCTION
			amqp_rpc_reply_t AMQP_CALL amqp_channel_close(amqp_connection_state_t state,
                                              amqp_channel_t channel, int code);
		关闭连接：
			AMQP_PUBLIC_FUNCTION
			amqp_rpc_reply_t AMQP_CALL amqp_connection_close(amqp_connection_state_t state,
			                                                 int code);
		清理资源：
			AMQP_PUBLIC_FUNCTION
			int AMQP_CALL amqp_destroy_connection(amqp_connection_state_t state);

###2. 默认的交换器
RabbitMQ 服务器默认会创建一些交换器，在 **RabbitMQ** 的安装目录下的 **sbin** 目录下运行 **rabbitmqctl.bat** 命令，并指定参数 **list_exchanges** ，可以查看默认创建的交换器:

![snipaste20180307_214426.png](https://i.loli.net/2018/03/07/5a9fecdaeb0e4.png)

上图中箭头指明的交换器和上下文中的其它交换器有不同，不同之处在于没有名字，在最开始接触 **RabbitMQ** 初期，对于我们能够跑通 demo 是很有用的。

> 关于默认交换器的文档见 [exchange-default][1]： 
> 
The default exchange is a direct exchange with no name (empty string) pre-declared by the broker. It has one special property that makes it very useful for simple applications: **every queue that is created is automatically bound to it with a routing key which is the same as the queue name**.

现在需要记住， RabbitMQ 服务器默认创建了一个类型为 direct ，名字为空的交换器。
在第一个 hello world 的官方例子中，我们没有自己去创建交换器，而是使用默认的交换器。

###3. 代码
生产者:

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
	DWORD WINAPI producer(LPVOID lpParam)
	{
		config_for_producer *config = (config_for_producer *)lpParam;

		// 1. 连接并登录到 RabbitMQ 服务器
		amqp_connection_state_t conn;
		{
			conn = amqp_new_connection();
	
			amqp_socket_t *socket = amqp_tcp_socket_new(conn);
			int status = amqp_socket_open(socket, config->host, config->port);
	
			amqp_login(conn
				, AMQP_DEFAULT_VHOST
				, AMQP_DEFAULT_MAX_CHANNELS
				, AMQP_DEFAULT_FRAME_SIZE
				, AMQP_DEFAULT_HEARTBEAT
				, AMQP_SASL_METHOD_PLAIN
				, config->user_name
				, config->user_password);
		}
	
		// 2. 获取信道
		int channel_id = config->channel_id;
		{
			amqp_channel_open(conn, channel_id);
			amqp_get_rpc_reply(conn);		// 这里其实需要对函数返回值进行判断，确保 amqp_channel_open 调用成功
		}
	
		// 3. 使用默认的交换器，所以这里不创建交换器

		// 4. 创建队列
		amqp_bytes_t queue_name;
		{
			amqp_queue_declare_ok_t *r = amqp_queue_declare(
				conn, channel_id, amqp_cstring_bytes(config->queue_name)	// 也可以传入 amqp_empty_bytes 使得 RabbitMQ 为我们自动分配一个队列名字
				, 0		// passive
				, 1		// durable
				, 0		// exclusive
				, 0		// auto_delete
				, amqp_empty_table);
			amqp_get_rpc_reply(conn);		// 这里其实需要对函数返回值进行判断，确保 amqp_queue_declare 调用成功
			queue_name = amqp_bytes_malloc_dup(r->queue);		// 获取队列名字
			//printf("生产者队列名称: %.*s\n", (int)queue_name.len, (char *)queue_name.bytes);
		}
	
		// 5. 绑定。
		/*
		 如果创建了交换器，那么这里需要传入交换器的名字，否则不需要我们显式绑定，因为创建队列的时候默认会绑定到默认交换器上。
		*/
		if (*config->exchange_name != NULL)
		{
			amqp_queue_bind(conn, channel_id, queue_name, amqp_cstring_bytes(config->exchange_name),
				amqp_cstring_bytes(config->bind_key), amqp_empty_table);
			amqp_get_rpc_reply(conn);
		}
	
		// 6. 推送消息
		const char *exchange_name = config->exchange_name;
		const char *message = "te测试st";
		{
			amqp_basic_properties_t props;
			props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
			props.content_type = amqp_cstring_bytes("text/plain");
			props.delivery_mode = AMQP_DELIVERY_PERSISTENT; /* persistent delivery mode */
			// 这里也需要对 amqp_basic_publish 的返回值进行判断，确保调用成功
			amqp_basic_publish(conn, channel_id, amqp_cstring_bytes(exchange_name),
				amqp_cstring_bytes(config->bind_key)
				, 1			// mandatory ，无法找到 queue 存储消息，那么 broker 会调用 basic.return 将消息返还给生产者
				, 0			// immediate ， exchange 在将消息 route 到 queue(s) 时发现对应的 queue 上没有消费者，那么这条消息会通过basic.return方法返还给生产者
				, &props, amqp_cstring_bytes(message));
		}
	
		// 7. 关闭清理
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


> 编译器编译和链接设置：
> 
> 编译设置：项目属性 -> C/C++  -> 常规 -> 附加包含目录，指定头文件
> 
> 链接设置：项目属性 -> 链接器 -> 常规 -> 附加库目录，指定 lib 文件目录，
> 项目属性 -> 链接器 -> 输入 -> 附加依赖项， lib 文件名
> 还需要将 dll 文件拷贝到项目所在目录



  [1]: https://www.rabbitmq.com/tutorials/amqp-concepts.html#exchange-default
  [2]: http://rabbitmq.1065348.n5.nabble.com/rabbitmq-c-how-many-channels-I-need-td27448.html#a27472
  [3]: http://rabbitmq.1065348.n5.nabble.com/Multithreading-and-rabbitmq-c-td19739.html#a19740