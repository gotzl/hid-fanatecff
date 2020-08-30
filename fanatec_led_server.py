#!/usr/bin/python3
import glob

sysfs_base = glob.glob("/sys/module/hid_fanatecff/drivers/hid:ftec_csl_elite/0003:0EB7:0005.*")[0]
sysfs_rpm = "%s/leds/0003:0EB7:0005.%s::RPM"%(sysfs_base,sysfs_base.split(".")[-1])

def set_leds(values):
    list(map(lambda i: open('%s%i/brightness'%(sysfs_rpm, i[0] + 1),'w').write(i[1]), enumerate(values)))

def set_display(value):
    open('%s/display'%sysfs_base,'w').write(str(value))

# clear leds and display
def clear():
    set_leds(['0']*9)
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
        