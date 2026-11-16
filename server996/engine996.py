#!/usr/bin/env python3
"""996 风格服务端启动器。

结构：
  Config/     引擎配置
  Share/      共享数据/备份
  Log/        日志
  GameGate/   客户端入口 7000
  LoginSrv/   账号服务 7001
  DBSrv/      存档备份 7002
  LogSrv/     日志服务 7003
  GameSrv/    GameServer 7100
"""
import json, os, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CFG = json.load(open(os.path.join(HERE, "Config", "engine.json"), encoding="utf-8"))
PY = sys.executable
procs = []

def start(name, cmd, cwd, log, stdin_pipe=False):
    f = open(os.path.join(HERE, "Log", log), "ab", buffering=0)
    kw = {"stdin": subprocess.PIPE} if stdin_pipe else {}
    p = subprocess.Popen(cmd, cwd=cwd, stdout=f, stderr=subprocess.STDOUT, **kw)
    procs.append((name, p, f))
    print("启动", name, "pid", p.pid, flush=True)

def shutdown(*_):
    # 先让 GameSrv 走控制台 exit：触发 Stop() -> SaveAll()
    for name, p, f in procs:
        if name == "GameSrv" and p.stdin is not None:
            try:
                p.stdin.write(b"exit\n"); p.stdin.flush()
            except Exception: pass
    # 给 GameSrv 最多 4 秒保存全部角色
    for _ in range(40):
        if all(p.poll() is not None for _, p, _ in procs):
            break
        time.sleep(0.1)
    for name, p, f in procs:
        if p.poll() is None:
            try: p.terminate()
            except Exception: pass
    time.sleep(0.4)
    for name, p, f in procs:
        if p.poll() is None:
            try: p.kill()
            except Exception: pass
        f.close()
    sys.exit(0)

if __name__ == "__main__":
    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)
    os.makedirs(os.path.join(HERE, "Share"), exist_ok=True)
    # 先 GameSrv
    start("GameSrv", [os.path.join(ROOT, "build", "GameServer")],
          os.path.join(HERE, "GameSrv"), "GameSrv.log", stdin_pipe=True)
    time.sleep(2)
    start("GameGate", [PY, os.path.join(HERE, "GameGate", "gate.py"),
                       "--port", str(CFG["GameGate"]), "--target-port", str(CFG["GameSrv"])],
          HERE, "GameGate.log")
    start("LoginSrv", [PY, os.path.join(HERE, "LoginSrv", "login_srv.py"),
                       "--port", str(CFG["LoginSrv"])], HERE, "LoginSrv.log")
    start("DBSrv", [PY, os.path.join(HERE, "DBSrv", "db_srv.py"),
                    "--port", str(CFG["DBSrv"]), "--seconds", str(CFG["backup_seconds"])],
          HERE, "DBSrv.log")
    start("LogSrv", [PY, os.path.join(HERE, "LogSrv", "log_srv.py"),
                     "--port", str(CFG["LogSrv"])], HERE, "LogSrv.log")
    print("996 结构已启动：Gate", CFG["GameGate"], "GameSrv", CFG["GameSrv"], flush=True)
    while True:
        time.sleep(2)
        for name, p, f in procs:
            if p.poll() is not None:
                print(name, "退出，代码", p.returncode, flush=True)
                shutdown()
