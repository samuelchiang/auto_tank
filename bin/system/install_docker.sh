
#https://docs.docker.com/install/linux/docker-ce/ubuntu/#targetText=Install%20from%20a%20package&targetText=Go%20to%20https%3A%2F%2Fdownload,version%20you%20want%20to%20install.

sudo apt-get update
sudo apt-get install apt-transport-https ca-certificates curl gnupg-agent software-properties-common
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
sudo apt-get install docker-ce docker-ce-cli containerd.io
sudo docker run --rm  hello-world
