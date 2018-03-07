# RabbitMQ官方例子之HelloWorld例子笔记

标签（空格分隔）： RabbitMQ

---

###1. 默认的交换器
RabbitMQ 服务器默认会创建一些交换器，在 **RabbitMQ** 的安装目录下的 **sbin** 目录下运行 **rabbitmqctl.bat** 命令，并指定参数 **list_exchanges** ，可以查看默认创建的交换器:
![snipaste20180307_214426.png](https://i.loli.net/2018/03/07/5a9fecdaeb0e4.png)
上图中箭头指明的交换器和上下文中的其它交换器有不同，不同之处在于没有名字，在最开始接触 **RabbitMQ** 初期，对于我们能够跑通 demo 是很有用的。

> 关于默认交换器的文档见 [exchange-default][1] 
The default exchange is a direct exchange with no name (empty string) pre-declared by the broker. It has one special property that makes it very useful for simple applications: **every queue that is created is automatically bound to it with a routing key which is the same as the queue name**.

现在需要记住， RabbitMQ 服务器默认创建了一个类型为 direct ，名字为空的交换器。
###2. 生产者流程
生产者创建消息并推送的流程如下所示(暂时知识有限，下面的流程很可能不是完全正确):

 - 连接 RabbitMQ 服务器

        Q: 同一个用户可以多次登录吗？
        A: 没有找到明确的资料。个人想法是可以，原因如下： RabbitMQ 的账号和密码和我们平时理解的应该是不一样，它代表的更像是一种权限控制，例如账号A只能读，那么只要用账号A连接都只能读。换个角度，例如移动端使用 RabbitMQ ，那么就得创建成千上万个账号，这明显是不合理的。
 - 获取信道
 
        Q: Channel 是否有数量限制？ 
        A: 肯定存在数量限制，我们的程序不会有那么大的消耗。
            如果想大量使用 Channel ，最好是控制好消息推送队列的粒度，然后设置不同的消费者去接收，并且每个消费者单独一个 Channel 。见图1.
        Q: Channel 一样会有什么影响吗？
        A: Channel 是和 connection 相关的，同一个连接中 Channel id不能一样，但是不同连接则可以一样。见下图2.
![snipaste20180307_233930.png](https://i.loli.net/2018/03/07/5aa007c25046b.png)
[图1出处][2]
![snipaste20180307_232738.png](https://i.loli.net/2018/03/07/5aa00521ac77f.png)
[图2出处][3]
 - 创建交换器
要素：

        名字
        类型：direct、topic、Fanout
        passive：为 true 表示检测交换器是否存在，存在则返回成功，不存在则返回错误
        durable：是否持久化
        auto-delete：是否自动删除
        internal：
        arguments：未知用途
    
 - 创建队列（首先查询所要创建的队列是否已经存在）
 - 将交换器与队列绑定
 - 推送消息
 - 关闭连接

  [1]: https://www.rabbitmq.com/tutorials/amqp-concepts.html#exchange-default
  [2]: http://rabbitmq.1065348.n5.nabble.com/rabbitmq-c-how-many-channels-I-need-td27448.html#a27472
  [3]: http://rabbitmq.1065348.n5.nabble.com/Multithreading-and-rabbitmq-c-td19739.html#a19740