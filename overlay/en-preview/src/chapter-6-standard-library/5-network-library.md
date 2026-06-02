# Network Library

## Ipv4Addr

`Ipv4Addr` is the data structure provided by BiSheng C to represent an IPv4 address. An example of its usage is as follows:

```c
void test() {
    struct Ipv4Addr ip = struct Ipv4Addr::new(192, 168, 0, 1);
    uint32_t bits = ip.to_bits();
    assert(bits == 0xc0a80001);
}
```

The public interfaces provided by `Ipv4Addr` and their corresponding usage examples are as follows:

|Public Interface|Functionality|Code Example|
|---|---|---|
|`_Safe struct Ipv4Addr struct Ipv4Addr::new(uint8_t a, uint8_t b, uint8_t c, uint8_t d)`|Creates an Ipv4Addr instance|`struct Ipv4Addr ip = struct Ipv4Addr::new(192, 168, 0, 1);`|
|`_Safe uint32_t struct Ipv4Addr::to_bits(This this)`|Converts an Ipv4Addr instance into a uint32_t value|`uint32_t bits = ip.to_bits();`|
