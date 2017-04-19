#include <zmq.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <QHash>
#include "stock_msg.h"
#include "zmqutils.h"
static int interrupted = 0;
void signal_handler (int sig)
{
    (void)sig;
    interrupted = 1;
}

/**************************
*函数名：catch_signals
*函数功能：捕获信号
*输入参数：无
*输出参数：无
*返回值：无
*作者：何必仕
*完成日期：2017-4-19
*修改日期：
***************************/

void catch_signals (void)
{
    struct sigaction action;
    action.sa_handler = signal_handler;
    action.sa_flags = 0;
    sigemptyset (&action.sa_mask);
    sigaction (SIGINT, &action, NULL);
    sigaction (SIGHUP, &action, NULL);
}



int main (void)
{
    catch_signals();
    //zeromq 上下文
    void *context = zmq_ctx_new();
    
    //PUB-SUB(发布与订阅者）
    void *subscriber = zmq_socket (context, ZMQ_SUB);
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
    zmq_bind (subscriber, "tcp://*:5555");

    void *publisher = zmq_socket (context, ZMQ_PUB);
    zmq_bind (publisher, "tcp://*:5556");

		//ROUTER-DEALER(共享队列）
    void *router = zmq_socket (context, ZMQ_ROUTER);
    zmq_bind (router, "tcp://*:5557");

    int ret;
    zmq_pollitem_t items[] = {{subscriber, 0, ZMQ_POLLIN, 0}
                              , {router, 0, ZMQ_POLLIN, 0}};

    QHash<uint, StockTick> hash;

    ZmqUtils::s_console("proxy sever start success\n");
    while (!interrupted)
    {

        ret = zmq_poll(items, sizeof(items) / sizeof(items[0]), -1);
        if(ret == -1)
            perror("zmq_poll");
        if(items[0].revents & ZMQ_POLLIN)
        {
            StockTick msg;
            zmq_recv(subscriber, &msg.StockCode, sizeof(msg.StockCode), 0);
            zmq_recv(subscriber, &msg.TimeStamp, sizeof(msg.TimeStamp), 0);
            zmq_recv(subscriber, &msg.LastPrice, sizeof(msg.LastPrice), 0);
            ZmqUtils::s_console("StockCode %d\n", msg.StockCode);
            zmq_send(publisher, &msg.StockCode, sizeof(msg.StockCode), ZMQ_SNDMORE);
            zmq_send(publisher, &msg.TimeStamp, sizeof(msg.TimeStamp), ZMQ_SNDMORE);
            zmq_send(publisher, &msg.LastPrice, sizeof(msg.LastPrice), 0);
            if(hash.contains(msg.StockCode))
                hash[msg.StockCode] = msg;
            else
                hash.insert(msg.StockCode, msg);
        }

        if(items[1].revents & ZMQ_POLLIN)
        {
            zmq_msg_t address, tmp, data;
            zmq_msg_init(&tmp);
            zmq_msg_init(&address);
            zmq_msg_init(&data);
            zmq_msg_recv(&address, router, 0);
            ZmqUtils::dump_msg(&address);
            zmq_msg_recv(&data, router, 0);
            ZmqUtils::dump_msg(&data);

            if(ZmqUtils::msg_cmp(&data, STOCK_CMD_SUB, STOCK_CMD_LEN))
            {
                ZmqUtils::s_console("new subscriber\n");
                QHash<uint, StockTick>::iterator it;

                for( it = hash.begin(); it != hash.end(); it++)
                {
                    zmq_msg_copy( &tmp, &address);
                    zmq_msg_send(&tmp, router, ZMQ_SNDMORE);
                    zmq_send(router, &(it.value().StockCode), sizeof(it.value().StockCode), ZMQ_SNDMORE);
                    zmq_send(router, &(it.value().TimeStamp), sizeof(it.value().TimeStamp), ZMQ_SNDMORE);
                    zmq_send(router, &(it.value().LastPrice), sizeof(it.value().LastPrice), 0);
                }
            }
            zmq_msg_close(&address);
            zmq_msg_close(&data);
        }
    }
    ZmqUtils::s_console("proxy sever end\n");
    zmq_close (router);
    zmq_close (publisher);
    zmq_close (subscriber);
    zmq_ctx_destroy(context);
    return 0;
}
