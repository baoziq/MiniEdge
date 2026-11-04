import socket
import time

with socket.create_connection(("127.0.0.1", 8001)) as s:
    s.settimeout(5)

    s.sendall(b"GET / HTTP/1.1\r\nHost: local")
    time.sleep(1)

    s.sendall(b"host\r\nConnection: close\r\n")
    time.sleep(1)

    s.sendall(b"\r\n")

    while data := s.recv(4096):
        print(data.decode(errors="replace"), end="")