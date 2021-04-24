import socket

with open("server-hosts") as f:
    server_hosts = f.readlines()

for i,k in enumerate(server_hosts):
    server_hosts[i] = server_hosts[i][:-1]

with open("client-hosts") as f:
    client_hosts = f.readlines()

for i,k in enumerate(client_hosts):
    client_hosts[i] = client_hosts[i][:-1]
    
print(server_hosts)
print(client_hosts)

server_ips = []
client_ips = []

for i in server_hosts:
    str = i + ".bftsmart.morty-pg0.utah.cloudlab.us"
    print(str)
    server_ips.append(socket.gethostbyname(str))
print(server_ips)
for i in client_hosts:
    str = i + ".bftsmart.morty-pg0.utah.cloudlab.us"
    print(str)
    client_ips.append(socket.gethostbyname(str))
print(client_ips)

with open("../store/bftsmartstore/library/java-config/hosts.config", 'w') as f:
    for i,k in enumerate(server_ips):
        f.write("%d %s 7088 7089\n" % (i, k))
    
    for i,k in enumerate(client_ips):
        f.write("%d %s 7088 7089\n" % (7000 + i, k))


