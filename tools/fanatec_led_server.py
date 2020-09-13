#!/usr/bin/python3
import glob
from pydbus import SystemBus

bus = SystemBus()
wheel = bus.get('org.fanatec.CSLElite', '/org/fanatec/CSLElite/Wheel')

def set_leds(values):
    global wheel
    wheel.RPM = values

def set_display(value):
    global wheel
    wheel.Display = value

# clear leds and display
def clear():
    set_leds([False]*9)
    set_display(-1)


if __name__ == "__main__":
    import threading
    from ac import AcClient
    from f1_2020 import F12020Client

    try:
        ev = threading.Event()
        # start f1 server
        f1 = F12020Client(ev)
        f1.start()

        # start ac client
        ac = AcClient(ev)
        ac.start()

        # run as long as the client threads are running, or CTRL+C
        print("Running ...")
        f1.join()
        ac.join()
            
    except (KeyboardInterrupt, SystemExit):
        print("Exiting")
    except Exception as e:
        raise e
    finally:
        ev.set()
        clear()
        
