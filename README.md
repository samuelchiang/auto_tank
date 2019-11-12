# Auto Tank
IoT Cloud Solution for Reef Tank Data Monitoring and Remote Control

## Overview

Use a ESP8266 board (Arduino compatible) with pH meter and temperature sensor to publish sensor data through MQTT. The MQTT broker is hosted on cloud. Using a Node-RED to subscribe the MQTT sensor data and store to InfluxDB. Grafana is used to show the historical sensor data, and set alert conditions for the value worthing notification. You can also ask Siri to report the last status of your reef tank.

![arch](./docs/arch/architect.png)

## Features

- Monitor historical pH value of your reef tank, including the two point calibration
- Monitor historical temperature of your reef tank
- Get the status of tank by Siri
- Get alert notification by Slack
- Low cost
- Private Cloud Solution

## Getting Started

### Prerequisites

* Local Side
	1. ESP8266 board with wifi
	1. Analog pH Meter Pro Sensor
	1. Calibration Solution pH7 and pH10
	1. DS18B20 waterproof temperature sensor
	1. Breadboard
	1. Breadboard connectors
	1. Micro usb cable and power adapter

* Cloud Side
	1. A server (CPU:1 Core, 1GB RAM at least)
	1. Public IP (If private ip, you can only access inside the local network. Use port forward to access on internet)
	1. [Install Docker Engine](https://docs.docker.com/install/) on the server

* For example, my cloud settings are as follows:
	1. Apply an AWS EC2 VM
	    1. Choose Ubuntu 16.04 t2.micro
	    2. Create Security Group. Add Rule for TCP, Source: Anywhere, with
	        1. MQTT:1883
	        2. NodeRed:1880
	        3. Grafana:3000
	        4. InfluxDB:8086
	    3. Instances > Actions > Networking > Change Security Groups.
	2. Install Docker ```./bin/system/install_docker.sh```
	3. Add Port in iptable ```./bin/system/add_port.sh ```
	4. Save iptables ```./bin/system/save_iptables.sh```

### Installing

A step by step series of examples that tell you how to get a development env running

Say what the step will be

```
Give the example
```

And repeat

```
until finished
```

End with an example of getting some data out of the system or using it for a little demo


## Built With

* [Dropwizard](http://www.dropwizard.io/1.0.2/docs/) - The web framework used
* [Maven](https://maven.apache.org/) - Dependency Management
* [ROME](https://rometools.github.io/rome/) - Used to generate RSS Feeds

## Contributing

Please read [CONTRIBUTING.md](https://gist.github.com/PurpleBooth/b24679402957c63ec426) for details on our code of conduct, and the process for submitting pull requests to us.

## Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/your/project/tags). 

## Authors

* **Billie Thompson** - *Initial work* - [PurpleBooth](https://github.com/PurpleBooth)

See also the list of [contributors](https://github.com/your/project/contributors) who participated in this project.

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

## Acknowledgments

* pH8.4
* Reef2Reef, worlds largest reef tank user forum 
* 齊 Siri
* KHG author for pH calibration

