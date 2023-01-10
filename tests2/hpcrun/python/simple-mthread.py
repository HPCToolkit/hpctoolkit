import contextlib
import os
import signal
import sys
import threading
import time


# Functions that "do work" that we will be detecting later
def func_hi() -> None:
    for i in range(10):
        func_mid()


def func_mid() -> None:
    func_lo()


def func_lo() -> None:
    time.sleep(0.1)


# Main loop(s)
ts = [threading.Thread(target=func_hi) for i in range(2)]
for t in ts:
    t.start()
for t in ts:
    t.join()
