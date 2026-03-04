# Systat - C version

## Description
A lightweight system monitoring daemon written in C that runs in the background to collect system performance metrics such as CPU load, memory consumption and uptime. The daemon exposes a local socket server, allowing client applications to securely connect and retrieve real-time metrics on demand (every 2 seconds).

### ⚙️ Requirements
- GCC
- Make
- A Unix-like system (Linux recommended)

### :hammer: Build 
To compile all binaries :
```bash
make all
```

### :rocket: Usage
<span style="background-color:#1E90FF; color:white; border-radius:3px; padding:2px 5px;">1</span> Start the daemon
```bash
./systat
```

The daemon will listen for local client connections via a socket.


<span style="background-color:#1E90FF; color:white; border-radius:3px; padding:2px 5px;">2</span> Run a client test 

```bash
./client_test
```

The client will ping the server every 2 seconds. The daemon will start collecting system metrics and listen for local client connections via a socket.
As the project is still in the development stage, the result is displayed in the console.

<span style="background-color:#1E90FF; color:white; border-radius:3px; padding:2px 5px;">3</span> Run test

```bash
./printstat
```
 
The client will ping the server and print result in your terminal only once.