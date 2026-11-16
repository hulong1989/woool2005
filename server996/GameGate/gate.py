#!/usr/bin/env python3
"""GameGate：客户端统一入口，转发到 GameSrv。"""
import argparse, socket, socketserver, threading, time, os

class Proxy(socketserver.BaseRequestHandler):
    target = ("127.0.0.1", 7100)

    def handle(self):
        try:
            up = socket.create_connection(self.target, timeout=5)
        except OSError:
            return
        self.request.settimeout(None); up.settimeout(None)
        def pump(src, dst):
            try:
                while True:
                    data = src.recv(65536)
                    if not data: break
                    dst.sendall(data)
            except OSError:
                pass
            finally:
                try: dst.shutdown(socket.SHUT_WR)
                except OSError: pass
        t = threading.Thread(target=pump, args=(up, self.request), daemon=True)
        t.start()
        pump(self.request, up)
        t.join(timeout=2)
        try: up.close()
        except OSError: pass

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=7000)
    ap.add_argument("--target-port", type=int, default=7100)
    a = ap.parse_args()
    Proxy.target = ("127.0.0.1", a.target_port)
    socketserver.ThreadingTCPServer.allow_reuse_address = True
    srv = socketserver.ThreadingTCPServer(("0.0.0.0", a.port), Proxy)
    print("GameGate listening", a.port, flush=True)
    srv.serve_forever()
