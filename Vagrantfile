# -*- mode: ruby -*-
# vi: set ft=ruby :

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  config.vm.box = "trusty64"
  config.vm.hostname = "greyhound-dev"
  config.vm.box_url = "https://vagrantcloud.com/ubuntu/trusty64/version/1/provider/virtualbox.box"
  config.vm.network :forwarded_port, guest: 80, host: 8080
  config.vm.provider :virtualbox do |vb|
	  vb.customize ["modifyvm", :id, "--memory", "2048"]
	  vb.customize ["modifyvm", :id, "--cpus", "2"]
	  vb.customize ["modifyvm", :id, "--ioapic", "on"]
  end

  ppaRepos = [
	  "ppa:ubuntugis/ubuntugis-unstable",
      "ppa:boost-latest/ppa"
  ]

  packageList = [
	  "git",
	  "build-essential",
	  "libjsoncpp-dev",
	  "pkg-config",
	  "redis-server",
	  "cmake",
	  "libflann-dev",
	  "libgdal-dev",
	  "libpq-dev",
	  "libproj-dev",
	  "libtiff4-dev",
	  "haproxy",
	  "libgeos-dev",
	  "python-all-dev",
	  "python-numpy",
	  "libxml2-dev",
	  "libboost-all-dev",
	  "libbz2-dev",
	  "libsqlite0-dev",
	  "cmake-curses-gui",
	  "screen",
	  "postgis",
	  "libcunit1-dev",
	  "postgresql-server-dev-9.3",
	  "postgresql-9.3-postgis-2.1",
      "libgeos++-dev",
      "node-gyp"
  ];

  nodeVersion = "0.10.29"
  nodeURL = "http://nodejs.org/dist/v#{nodeVersion}/node-v#{nodeVersion}-linux-x64.tar.gz"

  if Dir.glob("#{File.dirname(__FILE__)}/.vagrant/machines/default/*/id").empty?
	  pkg_cmd = ""

	  # provision node, from nodejs.org
	  pkg_cmd << "echo Provisioning node.js version #{nodeVersion}... ; mkdir -p /tmp/nodejs && \
		wget -qO - #{nodeURL} | tar zxf - --strip-components 1 -C /tmp/nodejs && cd /tmp/nodejs && \
		cp -r * /usr && rm -rf /tmp/nodejs ;"

	  pkg_cmd << "apt-get update -qq; apt-get install -q -y python-software-properties; "

	  if ppaRepos.length > 0
		  ppaRepos.each { |repo| pkg_cmd << "add-apt-repository -y " << repo << " ; " }
		  pkg_cmd << "apt-get update -qq; "
	  end

	  # install packages we need
	  pkg_cmd << "apt-get install -q -y " + packageList.join(" ") << " ; "
	  config.vm.provision :shell, :inline => pkg_cmd

	  scripts = [
		  "startup.sh",
		  "websocketpp.sh",
		  "libgeotiff.sh",
		  "nitro.sh",
		  "hexer.sh",
		  "p2g.sh",
		  "soci.sh",
		  "laszip.sh",
		  "pdal.sh"
	  ];
	  scripts.each { |script| config.vm.provision :shell, :path => "scripts/vagrant/" << script }

      # Install npm packages, build C++ code, launch Greyhound, stamp down a
      # sample pipeline ready for immediate use.
      config.vm.provision :shell, path: "set-stuff-up.sh"

      # Automatically cd to /vagrant on 'vagrant ssh'.
      config.vm.provision :shell, :inline => "echo \"\n\ncd /vagrant\n\" >> /home/vagrant/.bashrc"
  end
end
