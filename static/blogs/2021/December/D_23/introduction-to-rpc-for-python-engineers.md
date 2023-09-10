---
title: RPC fundamentals for Python Engineers
author: KHERA SHANU
create_date: 2021-12-23
update_date: 2021-12-29
tags:
  - Python
  - RPC
  - Distributed Systems
  - Data Engineering
category: Distributed Systems
description: "Dive into RPC in Python through this comprehensive but easy to read blog, exploring foundational concepts of remote procedure calls and how to implement them in Python."
thumb: rpc-python.webp
published: true
---

## Remote Procedure Call (RPC) in Distributed Computing: An Overview

When it comes to the world of distributed computing, achieving seamless communication and coordination between different services and components is a primary concern. The field has experimented with numerous technologies and protocols to facilitate this communication, and one of the most established ones is the Remote Procedure Call (RPC). In this blog, we'll dive deep into the concept of RPC, exploring its essence and understanding why it remains an integral part of distributed computing.

## What is RPC?

At its heart, RPC allows a program to cause a procedure (subroutine) to execute in another address space (commonly on another computer on a shared network). Simply put, it's a way for a program to request a service from another program located on a different computer in a network.

Think of RPC as if you're using a remote control to switch channels on your TV. Even though you're not physically pressing the buttons on the TV, you can change the channels. Similarly, with RPC, you're "remotely" calling functions that aren’t within the same address space as your program.

## How Does RPC Work?

1. **Procedure Stubs:**  For every remote procedure available for calling, there’s a local “stub” procedure in the calling environment. When a caller wants to execute a remote procedure, it calls this local stub.

2. **Message Packaging:**  The stub packages the parameters into a form that can be transmitted over a network, often referred to as marshalling.

3. **Remote Execution:**  The message is sent to the remote system. Upon receiving the message, it's unpacked, and the appropriate procedure is called using the unpacked parameters.

4. **Result Return:**  After the procedure has been executed on the remote system, the results are packaged, sent back over the network to the calling system, and returned to the caller.

## Why is RPC Vital in Distributed Computing?

1. **Abstraction:**  RPC abstracts the communication process between distributed components. Developers can focus on the core logic of their application without getting bogged down by the intricacies of network communication.

2. **Language Neutrality:**  Most RPC systems allow for different parts of a distributed application to be written in different programming languages. As long as they adhere to a predefined contract, they can communicate effectively.

3. **Efficiency:**  By allowing direct calls to remote procedures, RPC can be more efficient than other communication methods that might involve more overhead or intermediary steps.

4. **Flexibility:**  With RPC, you can evolve the distribution of your system components as needed. For instance, initially, you might have multiple components on the same machine, but as your system grows, you can move some of those components to different servers.

5. **Synchronization:**  RPC inherently supports synchronous operations. The caller sends a request and waits for a response, ensuring a level of coordination and synchronization.

6. **Structured Communication:**  RPC encourages structured communication using well-defined contracts (often called interfaces or service definitions). This clarity can improve system reliability and maintainability.

## Potential Challenges

While RPC brings numerous benefits, it's essential to be aware of potential challenges:

1. **Network Overhead:**  Relying heavily on RPCs can introduce significant network overhead, especially if the granularity of the calls is too fine.

2. **Latency:**  Remote calls are slower than local calls. If not managed properly, heavy use of RPCs can lead to latency issues.

3. **Error Handling:**  Handling errors in RPC can be more complex than in local computing, given that you need to manage issues like network failures, timeouts, or remote server crashes.

4. **Versioning:**  As systems evolve, maintaining backward compatibility in RPC interfaces can become a challenge, especially in large and rapidly evolving systems.

Remote Procedure Call (RPC) has stood the test of time as an invaluable tool in the realm of distributed computing. By providing a mechanism for programs to communicate across different systems seamlessly, it promotes efficiency, abstraction, and flexibility. Like all technologies, RPC has its challenges, but with careful design and consideration, it remains a cornerstone in building scalable and distributed applications.

## RPC in Python: Introduction and Basic Server-Client Setup

If You just didn't read the theory part above, let me me give You the important gist:

Remote Procedure Call (RPC) is a protocol that allows executing code on a remote server. It is like calling a function locally but the function runs somewhere else. Now, we'll explore the basics of RPC using Python and build a simple RPC server-client system.

In essence, RPC enables one to:

1. Define a set of functions on the server.

2. Expose these functions to be called remotely by clients.

3. Allow clients to call these functions as if they were local.

Benefits:

- Decoupling of services.

- Distribution of load.

- Flexibility in architecture decisions.

### Basic RPC with Python's `xmlrpc` Library

Python offers a built-in library called `xmlrpc` for RPC, which uses XML as a format for serializing the data.

#### Our RPC Server 🚀

First, we'll define our RPC server:

```python

from xmlrpc.server import SimpleXMLRPCServer

def add(a, b):
    return a + b

def subtract(a, b):
    return a - b

# Create a server instance
server = SimpleXMLRPCServer(("localhost", 9000))

# Register our functions
server.register_function(add, "add")
server.register_function(subtract, "subtract")

print("RPC Server is running on port 9000...")
server.serve_forever()
```

This creates an RPC server on `localhost` at port `9000` and exposes two functions, `add` and `subtract`.

#### The RPC Client 💻

Next, let's create our client that will make RPCs to our server:

```python

import xmlrpc.client

# Connect to our server
proxy = xmlrpc.client.ServerProxy("http://localhost:9000/")

# Call our remote procedures
print(proxy.add(5, 3))  # Outputs: 8
print(proxy.subtract(5, 3))  # Outputs: 2
```
With this, we've achieved a simple client-server model where the client can call functions on the server remotely.

## RPC in Python: Advanced Topics

We just explored the foundational aspects of RPC in Python using the `xmlrpc` library. Now, we'll delve deeper into some advanced topics such as error handling, authentication, and more.

### Error Handling in RPC

When making remote calls, errors can be due to various reasons such as network issues, server failures, or invalid data.

#### Server-Side Exceptions

On the server side, unhandled exceptions will automatically be sent back to the client:

```python

from xmlrpc.server import SimpleXMLRPCServer

def divide(a, b):
    return a / b

server = SimpleXMLRPCServer(("localhost", 9000))
server.register_function(divide, "divide")
server.serve_forever()
```

Here, if you call `divide` with b as 0, it will raise a `ZeroDivisionError`.

#### Handling Exceptions on the Client Side

On the client side, you can catch the exception and handle it gracefully:

```python

import xmlrpc.client

proxy = xmlrpc.client.ServerProxy("http://localhost:9000/")

try:
    print(proxy.divide(5, 0))
except xmlrpc.client.Fault as fault:
    print(f"Error {fault.faultCode}: {fault.faultString}")
```

### Authentication and Security

RPC over HTTP doesn't provide built-in security. For basic authentication, you can use HTTP Basic Authentication.

#### Basic Authentication on the Server Side

To implement basic authentication, we can subclass `SimpleXMLRPCRequestHandler`:

```python

from xmlrpc.server import SimpleXMLRPCServer, SimpleXMLRPCRequestHandler
import base64

class VerifyingRequestHandler(SimpleXMLRPCRequestHandler):
    def authenticate(self, headers):
        header = headers.get('Authorization')
        _, encoded = header.split()
        decoded = base64.b64decode(encoded).decode('utf-8')
        username, password = decoded.split(':')

        # This is just for You to experiment, don't use this in production 😁
        return username == "admin" and password == "password"

    def parse_request(self):
        if SimpleXMLRPCRequestHandler.parse_request(self):
            if self.authenticate(self.headers):
                return True
            else:
                self.send_error(401, 'Authentication failed')
                return False

server = SimpleXMLRPCServer(("localhost", 9000), requestHandler=VerifyingRequestHandler)
```

#### Client Side Authentication

On the client side, you can use `xmlrpc.client.ServerProxy` with authentication:

```python

proxy = xmlrpc.client.ServerProxy("http://admin:password@localhost:9000/")
```

### Advanced Data Serialization

By default, `xmlrpc` supports only basic data types. To handle custom objects, you can extend its Marshaller.

For example, to support `datetime.datetime` objects:

```python

from xmlrpc.client import SafeTransport, dumps, loads
import datetime

class DateTimeMarshaller:
    def dump_datetime(self, value, write):
        write(f"<value><dateTime.iso8601>{value.strftime('%Y%m%dT%H:%M:%S')}</dateTime.iso8601></value>")

    def load_datetime(self, node):
        return datetime.datetime.strptime(node.text, '%Y%m%dT%H:%M:%S')

# Extend xmlrpc's marshaller
marshaller = xmlrpc.client.Marshaller()
marshaller.dispatch[datetime.datetime] = DateTimeMarshaller().dump_datetime

transport = SafeTransport()
transport._marshaller = marshaller

proxy = xmlrpc.client.ServerProxy("http://localhost:9000/", transport=transport)
```

## RPC in Python: Performance, Asynchronicity, and Best Practices

Let's explore performance considerations, asynchronous RPC calls, and a few best practices for maintaining and scaling RPC systems in Python.

### Performance Optimizations

RPC can introduce latency due to the overhead of serialization, transmission, and deserialization. There are a few methods to optimize the performance:

#### Use Binary Serialization Formats

While XML is human-readable, it is also verbose. You can consider using binary serialization formats like `MessagePack` or `protobuf` to reduce the size of data in transit.

#### Compression

For large payloads, compression techniques can help in reducing the transmission time. Gzip is commonly used for this purpose.

### Asynchronous RPC Calls

Synchronous calls can block the client waiting for the server's response. Asynchronous RPC calls can be beneficial for long-running tasks or if the client needs to continue working without waiting.

#### Asynchronous Server

To make the server handle requests asynchronously, you can use threading or multiprocessing:

```python

from xmlrpc.server import SimpleXMLRPCServer
from xmlrpc.server import SimpleXMLRPCRequestHandler

class AsyncXMLRPCServer(SimpleXMLRPCServer):
    def _dispatch(self, method, params):
        func = self.funcs.get(method)
        if func is not None:
            return func(*params)
        else:
            raise Exception('Method %s is not supported' % method)

server = AsyncXMLRPCServer(("localhost", 9000), requestHandler=SimpleXMLRPCRequestHandler)
server.register_function(lambda x, y: x + y, "add")
server.serve_forever()
```

#### Asynchronous Client

On the client side, you can use Python's `asyncio` along with a library like `aiohttp` to make asynchronous calls.

Let's illustrate how to create an asynchronous RPC client using Python's `asyncio` and the `aiohttp` library. This client will communicate with our RPC server asynchronously.

**Asynchronous Client with `asyncio` and `aiohttp`**

To implement the asynchronous client, you'll first need to install the `aiohttp` library:

```bash

pip install aiohttp
```

Next, here's a simple asynchronous RPC client:

```python

import asyncio
import aiohttp
import xmlrpc.client

async def async_rpc_call(method_name, *params):
    url = "http://localhost:9000/"
    
    # Prepare the XML-RPC request payload
    payload = xmlrpc.client.dumps(params, method_name)

    async with aiohttp.ClientSession() as session:
        async with session.post(url, data=payload, headers={"Content-Type": "text/xml"}) as response:
            raw_response = await response.text()

            # Decode the XML-RPC response
            successful, result = xmlrpc.client.loads(raw_response)
            if successful:
                return result[0]
            else:
                raise Exception(result)

# Example usage
async def main():
    result = await async_rpc_call("add", 5, 3)
    print(f"5 + 3 = {result}")
    
    result = await async_rpc_call("subtract", 5, 3)
    print(f"5 - 3 = {result}")

asyncio.run(main())
```

In this client, we use `aiohttp` to make asynchronous HTTP requests. We send the XML-RPC request payload using a POST request to our RPC server and await its response. Once we receive the response, we decode it using `xmlrpc.client.loads()`. This allows us to perform RPC calls without blocking the main thread, which is especially useful for applications where multiple simultaneous RPC calls or other asynchronous operations are needed.

### 3. Best Practices

#### 3.1. Versioning

Always have a versioning mechanism in place. This ensures backward compatibility and smooth transitions when you make changes to the API.

#### 3.2. Logging and Monitoring

Implement logging on both the client and server sides. Monitoring the RPC system can help in identifying and fixing issues proactively.

#### 3.3. Scalability

Design your RPC server for scalability from the start. This might include:

- Load balancing: Distribute incoming network traffic across multiple servers.

- Caching: Use a cache mechanism to store results of expensive or frequently used function calls.

- Rate limiting: Prevent any individual client from overloading the server.

#### 3.4. Error Handling and Retries

Implement comprehensive error handling. If an RPC call fails due to a temporary glitch, consider retrying it after a short delay.

#### 3.5. Keep Payloads Small

Avoid sending large amounts of data in RPC calls. Instead, consider paginating results or using pointers to large data blobs that can be fetched separately.

Congratulations! 🥳 We delved into advanced considerations like performance, asynchronicity, and best practices. When implemented correctly, RPC can be a powerful tool for building distributed systems, but care should be taken to ensure scalability, resilience, and maintainability.

By mastering the concepts laid out in this blog, you'll be well-equipped to design, implement, and maintain robust RPC systems in Python.
