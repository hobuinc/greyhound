# -*- mode: ruby -*-
# vi: set ft=ruby :

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  config.vm.box = "precise64"

  config.vm.hostname = "point-serve"
  config.vm.box_url = "http://files.vagrantup.com/precise64.box"

  config.vm.network :forwarded_port, guest: 80, host: 8080

  #
  ppaRepos = [
	  "ppa:ubuntugis/ppa"
  ]

  packageList = [
	  "git",
	  "build-essential",
	  "libjsoncpp-dev",
	  "libboost1.48-all-dev",
	  "pkg-config",
	  "redis-server",
	  "cmake",
	  "libflann-dev",
	  "libgdal-dev",
	  "libpq-dev",
	  "libproj-dev",
	  "libtiff4-dev",
	  "libxml2-dev"
  ];

  nodeVersion = "0.8.23"
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

	  # install packages we need we need
	  pkg_cmd << "apt-get install -q -y " + packageList.join(" ") << " ; "
	  config.vm.provision :shell, :inline => pkg_cmd
  end
end
