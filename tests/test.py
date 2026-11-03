import socket
import time

with socket.create_connection(("127.0.0.1", 8001)) as sock:
    parts = [
        b"POST /test HTTP/1.1\r\n",
        b"Host: localhost\r\nContent-Length: 11\r\n",
        b"\r\nhello",
        b"=world",
    ]

    for i, part in enumerate(parts):
        print(f"Sending part {i + 1}: {part!r}")
        sock.sendall(part)
        time.sleep(1)

    sock.shutdown(socket.SHUT_WR)

    while data := sock.recv(4096):
        print(data.decode(errors="replace"), end="")