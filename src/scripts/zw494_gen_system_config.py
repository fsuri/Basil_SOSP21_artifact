hosts_dict = {
    "us-east-1-0": "10.10.1.1",
    "us-east-1-1": "10.10.1.3",
    "us-east-1-2": "10.10.1.5",
    "eu-west-1-0": "10.10.1.7",
    "eu-west-1-1": "10.10.1.9",
    "eu-west-1-2": "10.10.1.11",
    "ap-northeast-1-0": "10.10.1.13",
    "ap-northeast-1-1": "10.10.1.15",
    "ap-northeast-1-2": "10.10.1.17",
    "us-west-1-0": "10.10.1.19",
    "us-west-1-1": "10.10.1.21",
    "us-west-1-2": "10.10.1.23",
    "client-0-0": "10.10.1.2",
    "client-1-0": "10.10.1.4",
    "client-2-0": "10.10.1.6",
    "client-3-0": "10.10.1.8",
    "client-4-0": "10.10.1.10",
    "client-5-0": "10.10.1.12",
    "client-6-0": "10.10.1.14",
    "client-7-0": "10.10.1.16",
    "client-8-0": "10.10.1.18",
    "client-9-0": "10.10.1.20",
    "client-10-0": "10.10.1.22",
    "client-11-0": "10.10.1.24",
    "client-12-0": "10.10.1.26",
    "client-13-0": "10.10.1.28",
    "client-14-0": "10.10.1.30",
    "client-15-0": "10.10.1.32",
    "client-16-0": "10.10.1.34",
    "client-17-0": "10.10.1.36"
}

hosts_groups = [['us-east-1-0', 'eu-west-1-0', 'ap-northeast-1-0', 'us-west-1-0'],
                ['us-east-1-1', 'eu-west-1-1', 'ap-northeast-1-1', 'us-west-1-1'],
                ['us-east-1-2', 'eu-west-1-2', 'ap-northeast-1-2', 'us-west-1-2']]

for i, hostname in enumerate(hosts_dict):
    ipaddr = hosts_dict[hostname]
    with open("/home/zw494/BFT-DB/src/store/bftsmartstore/library/remote/java-config-" + hostname + "/java-config/system.config") as f:
        s = f.read()
        if "= auto" not in s:
            print("= auto does not exist in the file java-config-" + hostname)
            exit(0)
    with open("/home/zw494/BFT-DB/src/store/bftsmartstore/library/remote/java-config-" + hostname + "/java-config/system.config", "w") as f:
        s = s.replace("= auto", "= " + ipaddr)
        f.write(s)
        print("the content of s: ")
        print(s)
    with open("/home/zw494/BFT-DB/src/store/bftsmartstore/library/remote/java-config-" + hostname + "/java-config/hosts.config", "w") as f:
        k = i % 3
        for j in range(4):
            f.write("%d %s 30000 30001\n" % (j, hosts_dict[hosts_groups[k][j]]) )
        # for j in range(18):
        #     f.write("%d %s 30000 30001\n" % (j + 7000, hosts_dict["client-" + str(j) + "-0"]))
