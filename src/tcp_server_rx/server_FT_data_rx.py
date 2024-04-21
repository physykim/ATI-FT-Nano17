"""
File    : Server_FTLoadCell.py
Author  : Sooyeon Kim
Date    : March 27, 2024
Update  : 
Description : This script serves as a server to receive force/torque data from a client device 
              over TCP/IP. It expects data packets of 36 bytes:
              - 12 bytes for RDT sequence, FT sequence, and status
              - 24 bytes for force and torque data (Fx, Fy, Fz, Tx, Ty, Tz)
              Force values are in Newton (N) and torque values are in Newton millimeters (Nmm).
              The server decodes the received data, updates global variables, and logs the data 
              into a CSV file.
Protocol :
- TCP/IP communication.
- Each data packet: 12 bytes for sequence and status, 24 bytes for force/torque data.
- Units: Counts per force and torque (1000000) used for unit conversion.
- Continuous data reception, decoding, and logging.
- Press 'esc' to exit and close sockets.
"""


import socket
import struct
import keyboard
import time
from datetime import datetime
import threading
import csv

### Manually edit
csv_file_path = "C:/Users/user/Desktop/test.csv"

######################################################
###################### SETTING #######################
### Setting for TCP/IP communication
PORT = 4578
PACKET_SIZE = 36

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind(("0.0.0.0", PORT))
server_socket.listen(1) # Listen for incoming connections
print("Server waiting for client connection...")

client_socket, addr = server_socket.accept() # Accept a connection from a client
print(f"Connection from {addr}") 

# input("Press Enter to continue...")

### Data Initialize
received_FT_data = b''  # 빈 바이트 문자열로 초기화
status, rdt_sequence, ft_sequence = 0, 0, 0
ft_data = [0, 0, 0, 0, 0, 0]
counts_per_force, counts_per_torque = 1000000, 1000000


######################################################
################ FUNCTION AND THREAD #################
exit_flag = False

### Thread for TCP communication
def receive_FT_data():
    global exit_flag, received_FT_data
    global rdt_sequence, ft_sequence, status, ft_data

    while not exit_flag:
        chunk = client_socket.recv(PACKET_SIZE)

        if not chunk:  # Client connection failed
            print("[!] No data received.")
            exit_flag = True
            break
    
        received_FT_data = chunk

        # Encoding
        rdt_sequence, ft_sequence, status = struct.unpack('!III', received_FT_data[:12])
        ft_data = struct.unpack('!iiiiii', received_FT_data[12:36])
        ft_data = list(ft_data)

        for i in range(6):
            if i < 3:
                ft_data[i] = ft_data[i] / counts_per_force  # [N]
            else:
                ft_data[i] = ft_data[i] / counts_per_torque  # [Nmm]
        
        # Update data while holding the log_lock
        # with log_lock:
        #     rdt_sequence, ft_sequence, status = rdt_sequence, ft_sequence, status
        #     ft_data = ft_data

        # Print received data
        print(f"Received Data::{rdt_sequence}\t Fx: {ft_data[0]:.5f}\t Fy: {ft_data[1]:.5f}\t Fz: {ft_data[2]:.5f}\t Tx: {ft_data[3]:.5f}\t Ty: {ft_data[4]:.5f}\t Tz: {ft_data[5]:.5f})")

    else:
        client_socket.close()


### Thread for data logging
log_lock = threading.Lock() 
def log_data():
    global rdt_sequence, ft_sequence, status, ft_data

    with open(csv_file_path, mode='w', newline='') as csv_file:
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow(['RDT Sample Rate: 20'])
        csv_writer.writerow(['Force Units: N', 'Torque Units: Nmm'])
        csv_writer.writerow(['Time','Status', 'RDT Sequence', 'FT Sequence', 'Fx', 'Fy','Fz','Tx','Ty','Tz'])

        start_time = time.time()
        while not exit_flag:
            if time.time() - start_time >= 0.1: # 10Hz
                with log_lock: 
                    csv_writer.writerow([datetime.now(), status, rdt_sequence, ft_sequence, ft_data[0],ft_data[1],ft_data[2],ft_data[3],ft_data[4],ft_data[5]])

                    print(f"Received Data::{rdt_sequence}\t Fx: {ft_data[0]:.5f}\t Fy: {ft_data[1]:.5f}\t Fz: {ft_data[2]:.5f}\t Tx: {ft_data[3]:.5f}\t Ty: {ft_data[4]:.5f}\t Tz: {ft_data[5]:.5f})")

                start_time = time.time()

        else:
            csv_file.close()


######################################################
######################## MAIN ########################
######################################################
def main():

    global exit_flag
    threads = []

    ## Threads
    threads.append(threading.Thread(target=receive_FT_data))
    threads.append(threading.Thread(target=log_data))

    for thread in threads:
        thread.start()

    while not exit_flag:
        try:
            # Check keyboard inputs
            if keyboard.is_pressed('esc'):
                exit_flag = True
                print("Exit the loop")
                break
        except:
            pass

    for thread in threads:
        thread.join()



if __name__ == "__main__":
    main()




# Close
client_socket.close()
server_socket.close()
