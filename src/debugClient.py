import socket

# Specify the IP address and port to send data to
ip_address = "127.0.0.1"  # Replace with the target IP address
port = 69  # Replace with the target port number

# Create a UDP socket
udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Data to send
opcode = 1
filename = "file2.txt"  # Replace with the data you want to send
mode = "ocet"
# Convert the data to bytes
data_bytes = b''
data_bytes = data_bytes + opcode.to_bytes(2, "big")
data_bytes = data_bytes + filename.encode('utf-8')
data_bytes = data_bytes + b'\x00'
data_bytes = data_bytes + mode.encode('utf-8')
data_bytes = data_bytes + b'\x00'
print(data_bytes)
print(len(data_bytes))
# Send the data to the specified IP and port
udp_socket.sendto(data_bytes, (ip_address, port))

data, client_address = udp_socket.recvfrom(100)
print(data)
print(int.from_bytes(data[0:2], "big"))
print(int.from_bytes(data[2:4], "big"))
print(data[4:])
# Close the socket when done

    
udp_socket.close()