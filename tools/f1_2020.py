import socket
import threading
import time


class F12020Client(threading.Thread):
    UDP_IP = "127.0.0.1"
    UDP_PORT = 20777

    def __init__(self, ev): 
        threading.Thread.__init__(self)
        self.sock = sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((F12020Client.UDP_IP, F12020Client.UDP_PORT))
        self.sock.setblocking(0)
        self.ev = ev

    def run(self):
        import fanatec_led_server
        while not self.ev.isSet():
            try:
                data, addr = self.sock.recvfrom(2048)
                pkt_id = data[5]
                # only use telemetry packets
                if pkt_id != 6: continue
                car_idx = data[22]

                # start car delemetry data byte 24
                # size of car delemetry data 58
                car_telem_start = 24 + car_idx*58
                speed = int.from_bytes(data[car_telem_start:car_telem_start+2], byteorder="little")
                gear = int(data[car_telem_start+15])
                rpm = int.from_bytes(data[car_telem_start+16:car_telem_start+18], byteorder="little")
                suggestedGear = int(data[-1])

                fanatec_led_server.set_leds([False if (rpm-5000)<i*1000 else True for i in range(9)])
                fanatec_led_server.set_display(speed)
                # print(speed, rpm, gear)
            except:
                time.sleep(.5)

if __name__ == "__main__":
    try:
        ev = threading.Event()
        f1 = F12020Client(ev)
        f1.start()
        f1.join()

    except (KeyboardInterrupt, SystemExit):
        print("Exiting")
    except Exception as e:
        raise e
    finally:
        ev.set()
