import contextlib
import os
import signal
import sys
import threading
import time


# Functions that "do work" that we will be detecting later
def func_hi() -> None:
    func_mid()


def func_mid() -> None:
    func_lo()


def func_lo() -> None:
    time.sleep(0.1)


# Main loop
for _i in range(10):
    func_hi()
