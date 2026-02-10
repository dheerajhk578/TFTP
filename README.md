---

# 📁 TFTP File Transfer Project (C)

## 📌 Description

This project implements a **Simple TFTP (Trivial File Transfer Protocol)** client–server application using **C programming** and **UDP sockets**. It allows file transfer between a client and server through **GET** and **PUT** operations, demonstrating low-level network communication and protocol handling on Linux systems.

---

## 🚀 Features

* Client–Server architecture using UDP
* File upload (PUT) and download (GET)
* Supports octet (binary) mode
* Menu-driven client interface
* Error handling for invalid requests and missing files
* Lightweight and fast file transfer

---

## 🧠 Concepts Covered

* UDP socket programming
* TFTP protocol basics
* Packet types: RRQ, WRQ, DATA, ACK, ERROR
* File handling in C
* Modular programming

---

## 🛠️ Technologies Used

* **Language:** C
* **Protocol:** TFTP (UDP-based)
* **Platform:** Linux

---

## 📂 Project Structure

```
TFTP/
│── Client/
│   └── client.c
│── Server/
│   └── server.c
│── README.md
```

---

## ⚙️ Compilation

Use GCC to compile the client and server:

```bash
gcc client.c -o client
gcc server.c -o server
```

---

## ▶️ How to Run

### Start the Server

```bash
./server
```

### Run the Client

```bash
./client
```

Follow the menu options to connect, upload (PUT), or download (GET) files.

---

## 🎯 Learning Outcome

This project helps in understanding **UDP-based communication**, **client–server architecture**, and how lightweight protocols like TFTP work without TCP reliability mechanisms.

---

## 📌 Future Enhancements

* Timeout and retransmission handling
* Support for multiple clients
* Improved error logging
* Progress display for file transfer

---

## 👤 Author

**Dheeraj H K**

---
