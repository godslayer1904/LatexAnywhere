# 开机启动器：启动公式框并确认它真的就绪；没就绪（开机时 WebView2 / .NET 组件还没准备好等）就隔 5 秒重试，最多 5 次。
# 公式框的报错输出（pythonw 下原本会被吞掉）写进 boot-err-<主机名>.txt，日志写进 log-<主机名>.txt。
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.environ.get("COMPUTERNAME", "pc")
LOG = os.path.join(HERE, f"log-{HOST}.txt")
ERR = os.path.join(HERE, f"boot-err-{HOST}.txt")
PYW = os.path.join(os.path.dirname(sys.executable), "pythonw.exe")


def log(msg):
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(time.strftime("%H:%M:%S ") + "启动器：" + msg + "\n")


def ready_after(pos, timeout):
    """日志从 pos 之后出现「就绪」就算启动成功"""
    t = time.time()
    while time.time() - t < timeout:
        try:
            with open(LOG, "r", encoding="utf-8", errors="replace") as f:
                f.seek(pos)
                if "就绪" in f.read():
                    return True
        except OSError:
            pass
        time.sleep(0.5)
    return False


def main():
    for attempt in range(1, 6):
        pos = os.path.getsize(LOG) if os.path.exists(LOG) else 0
        log(f"第 {attempt} 次启动公式框")
        err = open(ERR, "a", encoding="utf-8")
        err.write(time.strftime("\n==== %Y-%m-%d %H:%M:%S 第 ") + str(attempt) + " 次 ====\n")
        err.flush()
        p = subprocess.Popen([PYW, os.path.join(HERE, "formula_box.py")], cwd=HERE, stdout=err, stderr=err,
                             creationflags=0x00000008 | 0x00000200)   # 脱离启动器（DETACHED_PROCESS | 新进程组）
        if ready_after(pos, 25):
            log("公式框已就绪")
            return
        code = p.poll()
        if code is None:
            # 进程还在但没就绪：可能是另一个实例已在运行（互斥锁让它退出前会先查到）——查日志里是否已有运行中的实例
            log("25 秒内未就绪，结束这次启动")
            p.kill()
        else:
            log(f"公式框退出（退出码 {code}），报错见 {os.path.basename(ERR)}")
            if code == 0:
                log("退出码 0：可能已有实例在运行，不再重试")
                return
        time.sleep(5)
    log("连续 5 次启动失败，放弃")


if __name__ == "__main__":
    main()
