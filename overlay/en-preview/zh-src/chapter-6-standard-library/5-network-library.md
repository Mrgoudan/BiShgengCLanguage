# 网络库

## Ipv4Addr

`Ipv4Addr`是 BiShengC 语言提供的表示 IPv4 地址的数据结构，其使用示例如下：

```c
void test() {
    struct Ipv4Addr ip = struct Ipv4Addr::new(192, 168, 0, 1);
    uint32_t bits = ip.to_bits();
    assert(bits == 0xc0a80001);
}
```

`Ipv4Addr`提供的对外接口及相应的使用用例如下：

|对外接口|接口功能|代码示例|
|---|---|---|
|`_Safe struct Ipv4Addr struct Ipv4Addr::new(uint8_t a, uint8_t b, uint8_t c, uint8_t d)`|创建 Ipv4Addr 实例|`struct Ipv4Addr ip = struct Ipv4Addr::new(192, 168, 0, 1);`|
|`_Safe uint32_t struct Ipv4Addr::to_bits(This this)`|将 Ipv4Addr 实例转换成 uint32_t 类型|`uint32_t bits = ip.to_bits();`|
