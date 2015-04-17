# -*- mode: ruby -*-
# vi: set ft=ruby :

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

$enable_serial_logging = false

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
    config.vm.box = "trusty64"
    config.vm.hostname = "greyhound-dev"
    config.vm.box_url = "https://cloud-images.ubuntu.com/vagrant/trusty/current/trusty-server-cloudimg-amd64-vagrant-disk1.box"
    config.vm.network :forwarded_port, guest: 8080, host: 8080
    config.vm.provider :virtualbox do |vb|
        vb.customize ["modifyvm", :id, "--memory", "2048"]
        vb.customize ["modifyvm", :id, "--cpus", "2"]
        vb.customize ["modifyvm", :id, "--ioapic", "on"]

        scripts = [
            "ubuntu.sh",
            "startup.sh",
            "websocketpp.sh",
            "libgeotiff.sh",
            "nitro.sh",
            "hexer.sh",
            "p2g.sh",
            "laszip.sh",
            "lazperf.sh",
            "pdal.sh"
        ];

        scripts.each {
            |script| config.vm.provision :shell,
            :path => "scripts/vagrant/" << script
        }

        config.vm.provision :shell, :path => "scripts/vagrant/vagrant.sh"

        # Automatically cd to /vagrant on 'vagrant ssh'.
        config.vm.provision :shell, :inline =>
            "echo \"\n\ncd /vagrant\n\" >> /home/vagrant/.bashrc"
    end
end

