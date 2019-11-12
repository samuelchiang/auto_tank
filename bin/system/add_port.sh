sudo iptables -A INPUT -p tcp --dport 1883 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 3000 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 8086 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 1880 -j ACCEPT

