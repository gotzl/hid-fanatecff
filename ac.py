import socket
import struct
import select
import threading

class AcClient(threading.Thread):
    UDP_IP = "127.0.0.1"
    UDP_PORT = 9996

    HANDSHAKE = 0
    SUBSCRIBE_UPDATE = 1
    SUBSCRIBE_SPOT = 2
    DISMISS = 3 

    def __init__(self, ev): 
        threading.Thread.__init__(self)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setblocking(0)
        self.ev = ev

    @staticmethod
    def client_data(sock, operation):
        sock.sendto(bytearray([
            0x0,0x0,0x0,0x2, # android phone
            0x0,0x0,0x0,0x0, # version
            operation,0x0,0x0,0x0, 
        ]), (AcClient.UDP_IP, AcClient.UDP_PORT))

    def run(self):
        import fanatec_led_server
        while not self.ev.isSet():
            # handshake
            AcClient.client_data(self.sock, AcClient.HANDSHAKE)

            # response
            ready = select.select([self.sock], [], [], 2)
            if not ready[0]:
                # print("Timeout connecting to AC server")
                continue
            data, addr = self.sock.recvfrom(2048)
            
            # confirm
            AcClient.client_data(self.sock, AcClient.SUBSCRIBE_UPDATE)   

            timeout_cnt = 0
            while not self.ev.isSet():
                ready = select.select([self.sock], [], [], .5)
                if not ready[0]:
                    print('Timeout waiting for AC server data,', timeout_cnt)
                    timeout_cnt+=1
                    if timeout_cnt>10:
                        print('Quitting AC server')
                        break
                    continue

                timeout_cnt = 0
                data, addr = self.sock.recvfrom(2048)
                speed_kmh = int(struct.unpack('<f', data[8:12])[0])
                rpm = int(struct.unpack('<f', data[68:72])[0])
                gear = int.from_bytes(data[76:80], byteorder='little')
                
                fanatec_led_server.set_leds(["0" if rpm<i*1000 else "1" for i in range(9)])
                fanatec_led_server.set_display(speed_kmh)
                # print(speed_kmh, rpm, gear)
                
            AcClient.client_data(self.sock, AcClient.DISMISS)
            fanatec_led_server.clear()
            self.sock.setblocking(1)

if __name__ == "__main__":
    try:
        ev = threading.Event()
        ac = AcClient(ev)
        ac.start()
        ac.join()

    except (KeyboardInterrupt, SystemExit):
        print("Exiting")
    except Exception as e:
        raise e
    finally:
        ev.set()
