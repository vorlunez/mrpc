# phase1 —— 单CS结构

目标：完成下面逻辑闭环
client http req -> server http parser -> router -> 业务逻辑 -> respons
1. mlog异步日志系统
2. epoll 接收单个client tcp字节流
3. 支持http编解码
4. 支持路由回调
验证：可以跑通单个请求，完成结果计算与回复

# phase2 —— 多client管理

目标：支持多client同时发req
1. 线程池维护多个client连接，用户任务队列与线程绑定
验收标准：
- 多个客户端交替发送请求，都能得到正确结果。
- 一条请求拆成多次发送，仍能正确解析。
- 一个客户端断开，其他连接继续正常工作。
- 未知路由和非法请求返回明确错误

# phase3 —— 数据支持json

目标：http payload可以用json编码

# phase4 —— client 服务治理、

客户端 Stub → 静态配置多个实例 → 负载均衡 → 超时 → 有限重试 → 服务发现 → 熔断
目标：client端
1. 服务发现stub
2. 负载均衡器
3. 重试策略
4. 熔断器
